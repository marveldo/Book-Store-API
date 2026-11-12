#include "PdfExtractor.hpp"
#include <mupdf/fitz.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <exception>
#include <map>
#include <optional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

using nlohmann::json;

namespace {

constexpr size_t STORE_BYTES     = 32 * 1024 * 1024;  // MuPDF's cache; its default is 256 MB
constexpr size_t MAX_TOC         = 2000;
constexpr size_t MAX_TOC_TITLE   = 500;
constexpr size_t MAX_IMAGE_BYTES = 80 * 1024 * 1024;  // all images of one book, kept in memory until uploaded
constexpr float  PAGE_WIDTH_PX   = 1200;              // rendered pages are at most this wide
constexpr float  IMAGE_SIDE_PX   = 1600;              // pictures are at most this big on their longest side

// ---------------------------------------------------------------------------
// MuPDF reports errors with setjmp/longjmp (fz_try / fz_catch), not C++ exceptions.
// Each helper below wraps the MuPDF calls that can fail and turns the failure into a
// PdfError. Keep them free of C++ objects with destructors: a longjmp skips destructors.
// Variables set inside fz_try are `volatile` because their value after a longjmp is
// otherwise undefined.
// ---------------------------------------------------------------------------

[[noreturn]] void fail(fz_context* ctx, const char* what) {
    std::string message = std::string(what) + ": " + fz_caught_message(ctx);
    throw PdfError(message);
}

fz_document* openDocument(fz_context* ctx, const char* path) {
    fz_document* volatile doc = nullptr;
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, path);
    }
    fz_catch(ctx) fail(ctx, "Could not open the PDF");
    return doc;
}

bool isLocked(fz_context* ctx, fz_document* doc) {
    volatile int locked = 0;
    fz_try(ctx) {
        // Many PDFs are "encrypted" with an empty password just to block copying. Those open fine.
        locked = fz_needs_password(ctx, doc) && !fz_authenticate_password(ctx, doc, "");
    }
    fz_catch(ctx) fail(ctx, "Could not read the PDF");
    return locked != 0;
}

int countPages(fz_context* ctx, fz_document* doc) {
    volatile int count = 0;
    fz_try(ctx) count = fz_count_pages(ctx, doc);
    fz_catch(ctx) fail(ctx, "Could not count the PDF's pages");
    return count;
}

fz_page* loadPage(fz_context* ctx, fz_document* doc, int index) {
    fz_page* volatile page = nullptr;
    fz_try(ctx) page = fz_load_page(ctx, doc, index);
    fz_catch(ctx) fail(ctx, "Could not read a page of the PDF");
    return page;
}

fz_rect pageBounds(fz_context* ctx, fz_page* page) {
    fz_rect bounds = fz_empty_rect;
    fz_try(ctx) bounds = fz_bound_page(ctx, page);
    fz_catch(ctx) fail(ctx, "Could not measure a page of the PDF");
    return bounds;
}

fz_stext_page* loadText(fz_context* ctx, fz_page* page) {
    fz_stext_page* volatile text = nullptr;
    fz_stext_options options{};
    options.flags = FZ_STEXT_PRESERVE_IMAGES;   // report pictures as blocks, in reading order
    fz_try(ctx) text = fz_new_stext_page_from_page(ctx, page, &options);
    fz_catch(ctx) fail(ctx, "Could not read a page of the PDF");
    return text;
}

// Draws part of a page (or all of it) into a picture, white background. RGB unless `gray`.
// Drawing the page area rather than copying the embedded image handles every colour
// space, mask and rotation the PDF might use.
fz_pixmap* renderArea(fz_context* ctx, fz_page* page, fz_rect area, float zoom, bool gray = false) {
    fz_pixmap* volatile pix = nullptr;
    fz_device* volatile dev = nullptr;
    fz_try(ctx) {
        const fz_matrix ctm = fz_scale(zoom, zoom);
        fz_colorspace* colorspace = gray ? fz_device_gray(ctx) : fz_device_rgb(ctx);
        pix = fz_new_pixmap_with_bbox(ctx, colorspace, fz_round_rect(fz_transform_rect(area, ctm)), nullptr, 0);
        fz_clear_pixmap_with_value(ctx, pix, 255);
        dev = fz_new_draw_device(ctx, fz_identity, pix);   // the zoom goes to fz_run_page only
        fz_run_page(ctx, page, dev, ctm, nullptr);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) fz_drop_device(ctx, dev);
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pix);
        fail(ctx, "Could not draw a page of the PDF");
    }
    return pix;
}

// PNG, not JPEG: MuPDF 1.23's JPEG writer (Ubuntu's version) saves RGB pictures with wrong
// colours, and its signature changed in later versions. Cloudinary converts the PNG to WebP
// when it's uploaded, so the size we store stays small.
fz_buffer* encodePng(fz_context* ctx, fz_pixmap* pix) {
    fz_buffer* volatile buffer = nullptr;
    fz_try(ctx) buffer = fz_new_buffer_from_pixmap_as_png(ctx, pix, fz_default_color_params);
    fz_catch(ctx) fail(ctx, "Could not encode an image from the PDF");
    return buffer;
}

fz_outline* loadOutline(fz_context* ctx, fz_document* doc) {
    fz_outline* volatile outline = nullptr;
    fz_try(ctx) outline = fz_load_outline(ctx, doc);
    fz_catch(ctx) outline = nullptr;   // a broken outline shouldn't fail the whole book
    return outline;
}

int outlinePage(fz_context* ctx, fz_document* doc, fz_outline* item) {
    volatile int page = -1;
    fz_try(ctx) page = fz_page_number_from_location(ctx, doc, item->page);
    fz_catch(ctx) page = -1;
    return page;
}

// Frees MuPDF objects when they go out of scope, including when a PdfError is thrown.
struct Context {
    fz_context*  ctx = nullptr;
    fz_document* doc = nullptr;
    ~Context() {
        if (doc) fz_drop_document(ctx, doc);
        if (ctx) fz_drop_context(ctx);
    }
};
struct Page     { fz_context* ctx; fz_page* page;        ~Page()     { fz_drop_page(ctx, page); } };
struct TextPage { fz_context* ctx; fz_stext_page* page;  ~TextPage() { fz_drop_stext_page(ctx, page); } };
struct Outline  { fz_context* ctx; fz_outline* root;     ~Outline()  { if (root) fz_drop_outline(ctx, root); } };
struct Pixmap   { fz_context* ctx; fz_pixmap* pix;       ~Pixmap()   { fz_drop_pixmap(ctx, pix); } };
struct Buffer   { fz_context* ctx; fz_buffer* buf;       ~Buffer()   { fz_drop_buffer(ctx, buf); } };

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

// Characters we never store: control characters, lone surrogates, and anything
// that isn't valid Unicode. Postgres rejects invalid UTF-8, and \0 in particular.
bool keep(int c) {
    if (c == '\t' || c == '\n' || c == '\r') return true;   // turned into spaces
    if (c < 0x20 || (c >= 0x7F && c <= 0x9F)) return false;
    if (c >= 0xD800 && c <= 0xDFFF) return false;
    if (c == 0xFFFE || c == 0xFFFF || c > 0x10FFFF) return false;
    return true;
}

void appendUtf8(std::string& out, int c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

// Outline titles come from MuPDF as UTF-8 already, but the PDF could contain anything.
std::string cleanUtf8(std::string_view in, size_t maxBytes) {
    static constexpr int MINIMUM[] = {0, 0x80, 0x800, 0x10000};   // smallest value each length may encode
    std::string out;
    size_t i = 0;
    while (i < in.size() && out.size() < maxBytes) {
        const unsigned char b = in[i];
        int c = 0, extra = 0;
        if      (b < 0x80)           { c = b;        extra = 0; }
        else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { c = b & 0x07; extra = 3; }
        else { i++; continue; }   // not a valid first byte

        bool valid = i + extra < in.size();
        for (int k = 1; valid && k <= extra; k++) {
            const unsigned char cont = in[i + k];
            if ((cont & 0xC0) != 0x80) valid = false;
            else c = (c << 6) | (cont & 0x3F);
        }
        if (!valid) { i++; continue; }

        if (c >= MINIMUM[extra] && keep(c)) {
            appendUtf8(out, (c == '\t' || c == '\n' || c == '\r') ? ' ' : c);
        }
        i += extra + 1;
    }
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    return out.substr(first, out.find_last_not_of(' ') - first + 1);
}

struct Run {
    std::string text;
    bool bold;
    bool italic;
};

// One thing on a page, in reading order: a block of text or a picture.
struct Block {
    bool        isImage = false;
    fz_rect     rect{};          // position on the page
    std::string imageId;         // public id, once the picture has been rendered
    bool        isFigure = false;// a drawing found by findFigures, not an embedded picture
    std::vector<fz_rect> glyphs; // each letter's box, only while looking for figures

    std::vector<Run> runs;       // text blocks only
    double sizeSum = 0;
    size_t chars   = 0;
    int    lines   = 0;
    int    numberedLines = 0;    // lines ending in a page number ("Chapter 3 .... 47")

    double size() const { return chars ? sizeSum / chars : 0; }

    std::string text() const {
        std::string all;
        for (const auto& run : runs) all += run.text;
        return all;
    }

    bool endsWith(char c) const {
        return !runs.empty() && !runs.back().text.empty() && runs.back().text.back() == c;
    }

    void addSpace() {
        if (!runs.empty() && !endsWith(' ')) runs.back().text += ' ';
    }

    void addChar(int c, bool bold, bool italic, float size) {
        if (runs.empty() || runs.back().bold != bold || runs.back().italic != italic) {
            runs.push_back(Run{"", bold, italic});
        }
        appendUtf8(runs.back().text, c);
        sizeSum += size;
        chars++;
    }

    void trim() {
        while (!runs.empty()) {
            auto& last = runs.back().text;
            while (!last.empty() && last.back() == ' ') last.pop_back();
            if (!last.empty()) break;
            runs.pop_back();
        }
    }
};

enum class PageKind { Text, Image, Cover, Skip, Blank };

struct RawPage {
    PageKind kind = PageKind::Text;
    std::vector<Block> blocks;
    fz_rect bounds{};
    std::string imageId;   // Image pages: the rendered page
};

struct FontStyle {
    bool bold;
    bool italic;
};

FontStyle styleOf(fz_context* ctx, fz_font* font, std::unordered_map<fz_font*, FontStyle>& cache) {
    if (!font) return {false, false};
    auto it = cache.find(font);
    if (it != cache.end()) return it->second;

    // Font flags are often missing, so fall back to the font's name ("Times-BoldItalic").
    const std::string_view name = fz_font_name(ctx, font);
    const auto has = [&](std::string_view word) { return name.find(word) != std::string_view::npos; };
    FontStyle style{
        fz_font_is_bold(ctx, font) || has("Bold") || has("Black") || has("Heavy") || has("Semibold"),
        fz_font_is_italic(ctx, font) || has("Italic") || has("Oblique")
    };
    cache.emplace(font, style);
    return style;
}

bool isLowercaseLetter(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 0xDF && c <= 0xFF && c != 0xF7);
}

bool isDigits(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// Page numbers printed in headers/footers ("12") aren't content.
bool isPageNumber(const std::string& text) {
    return text.size() <= 4 && isDigits(text);
}

// "Chapter 3 ........ 47" or "Preface ..... xiii": the last word is a page number.
bool endsWithPageNumber(const std::string& line) {
    const auto end = line.find_last_not_of(' ');
    if (end == std::string::npos) return false;
    const auto start = line.find_last_of(' ', end);
    std::string_view last(line.data() + (start == std::string::npos ? 0 : start + 1),
                          end - (start == std::string::npos ? 0 : start + 1) + 1);
    while (!last.empty() && last.front() == '.') last.remove_prefix(1);   // dot leaders stuck to the number
    if (isDigits(last) && last.size() <= 4) return true;
    return !last.empty() && last.size() <= 6 &&
           std::all_of(last.begin(), last.end(), [](char c) { return std::string_view("ivxlc").find(c) != std::string_view::npos; });
}

float area(const fz_rect& r) {
    return std::max(0.0f, r.x1 - r.x0) * std::max(0.0f, r.y1 - r.y0);
}

RawPage readPage(fz_context* ctx, fz_stext_page* page, fz_rect bounds, std::map<long, size_t>& sizes) {
    RawPage raw;
    raw.bounds = bounds;
    std::unordered_map<fz_font*, FontStyle> fonts;

    for (fz_stext_block* b = page->first_block; b; b = b->next) {
        if (b->type == FZ_STEXT_BLOCK_IMAGE) {
            Block picture;
            picture.isImage = true;
            picture.rect = fz_intersect_rect(b->bbox, bounds);
            raw.blocks.push_back(picture);
            continue;
        }
        if (b->type != FZ_STEXT_BLOCK_TEXT) continue;

        Block block;
        block.rect = b->bbox;
        bool lineBreak = false;
        for (fz_stext_line* line = b->u.t.first_line; line; line = line->next) {
            if (!block.runs.empty()) lineBreak = true;
            std::string lineText;   // plain copy, only to spot page numbers at the end

            for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                int c = ch->c;
                if (!keep(c)) continue;
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0) {
                    block.addSpace();
                    lineText += ' ';
                    continue;
                }

                if (lineBreak) {
                    // "exam-" + "ple" on the next line becomes "example".
                    if (block.endsWith('-') && isLowercaseLetter(c)) {
                        block.runs.back().text.pop_back();
                    } else {
                        block.addSpace();
                    }
                    lineBreak = false;
                }

                const FontStyle style = styleOf(ctx, ch->font, fonts);
                block.addChar(c, style.bold, style.italic, ch->size);
                block.glyphs.push_back(fz_rect_from_quad(ch->quad));
                sizes[std::lround(ch->size * 2)]++;
                if (c < 0x80) lineText += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
            }

            block.lines++;
            if (endsWithPageNumber(lineText)) block.numberedLines++;
        }

        block.trim();
        if (block.chars == 0 || isPageNumber(block.text())) continue;
        raw.blocks.push_back(std::move(block));
    }
    return raw;
}

// ---------------------------------------------------------------------------
// Deciding what kind of page this is
// ---------------------------------------------------------------------------

struct PageStats {
    size_t chars = 0;
    int    textBlocks = 0;
    int    tinyBlocks = 0;      // 1-2 characters: a letter on its own
    int    words = 0;
    int    oneLetterWords = 0;
    int    longWords = 0;       // 25+ letters: words glued together
    int    lines = 0;
    int    numberedLines = 0;
    bool   contentsHeading = false;
    float  imageShare = 0;      // part of the page covered by pictures (0..1)
};

PageStats statsOf(const RawPage& page) {
    PageStats s;
    float imageArea = 0;
    for (const auto& block : page.blocks) {
        if (block.isImage) { imageArea += area(block.rect); continue; }

        const std::string text = block.text();
        s.chars += block.chars;
        s.textBlocks++;
        if (block.chars <= 2) s.tinyBlocks++;
        s.lines += block.lines;
        s.numberedLines += block.numberedLines;

        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find(' ', start);
            if (end == std::string::npos) end = text.size();
            const size_t length = end - start;
            if (length > 0) {
                s.words++;
                if (length == 1) s.oneLetterWords++;
                if (length >= 25) s.longWords++;
            }
            start = end + 1;
        }

        // "Contents", "Brief Contents", "Contents in Detail", "Table of Contents"
        if (s.textBlocks <= 3 && text.size() <= 40) {
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
            if (lower.find("contents") != std::string::npos) s.contentsHeading = true;
        }
    }
    const float pageArea = area(page.bounds);
    s.imageShare = pageArea > 0 ? std::min(1.0f, imageArea / pageArea) : 0;
    return s;
}

// Decorative type (covers, title pages): letters one per line, letter-spaced titles that
// come out as one giant word, or "A N I N T R O ..." one letter at a time.
bool looksLikeGarbage(const PageStats& s) {
    if (s.textBlocks >= 6 && s.tinyBlocks * 100 >= s.textBlocks * 35) return true;
    if (s.words >= 10 && s.oneLetterWords * 2 >= s.words) return true;
    return s.longWords >= 2;
}

PageKind classify(const PageStats& s, int index, int pageCount, bool hasOutline, bool previousWasToc) {
    const bool garbage = looksLikeGarbage(s);
    if (index == 0 && (garbage || s.imageShare >= 0.25f)) return PageKind::Cover;

    // The book's printed table of contents. Only dropped when the PDF has an outline,
    // because that is what fills book_toc instead.
    const bool early = index < std::max(10, pageCount * 15 / 100);
    if (hasOutline && early && s.lines >= 5 && s.numberedLines * 10 >= s.lines * 4 &&
        (s.contentsHeading || previousWasToc)) {
        return PageKind::Skip;
    }

    if (garbage) return PageKind::Image;
    if (s.imageShare >= 0.6f && s.chars < 400) return PageKind::Image;   // a full-page picture with a caption
    return PageKind::Text;
}

// ---------------------------------------------------------------------------
// Running headers and footers ("www.itpub.net", the chapter title on every page)
// ---------------------------------------------------------------------------

bool inMargin(const Block& block, const fz_rect& bounds) {
    const float height = bounds.y1 - bounds.y0;
    return block.rect.y1 <= bounds.y0 + height * 0.09f || block.rect.y0 >= bounds.y1 - height * 0.09f;
}

// Same line on different pages: ignore case and the page number in it.
std::string marginKey(const Block& block) {
    std::string key;
    for (char c : block.text()) {
        if (c >= '0' && c <= '9') { if (key.empty() || key.back() != '#') key += '#'; }
        else key += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    return key;
}

void removeRunningHeaders(std::vector<RawPage>& pages) {
    int textPages = 0;
    std::unordered_map<std::string, int> seen;   // key -> pages it appears on
    for (const auto& page : pages) {
        if (page.kind != PageKind::Text) continue;
        textPages++;
        std::unordered_set<std::string> onThisPage;
        for (const auto& block : page.blocks) {
            if (!block.isImage && block.chars <= 120 && inMargin(block, page.bounds)) onThisPage.insert(marginKey(block));
        }
        for (const auto& key : onThisPage) seen[key]++;
    }

    const int threshold = std::max(3, textPages * 30 / 100);
    for (auto& page : pages) {
        if (page.kind != PageKind::Text) continue;
        std::erase_if(page.blocks, [&](const Block& block) {
            return !block.isImage && block.chars <= 120 && inMargin(block, page.bounds) && seen[marginKey(block)] >= threshold;
        });
    }
}

// ---------------------------------------------------------------------------
// TipTap
// ---------------------------------------------------------------------------

// The most common font size in the book is the body text. Noticeably bigger,
// short blocks are headings.
int headingLevel(const Block& block, double bodySize) {
    if (bodySize <= 0 || block.chars > 200) return 0;
    const double ratio = block.size() / bodySize;
    if (ratio >= 1.8)  return 1;
    if (ratio >= 1.45) return 2;
    if (ratio >= 1.2)  return 3;
    return 0;
}

json imageNode(const std::string& src, const std::string& alt) {
    return {{"type", "image"}, {"attrs", {{"src", src}, {"alt", alt}, {"title", nullptr}}}};
}

// TipTap / ProseMirror document: {"type":"doc","content":[ paragraphs, headings, images ]}
std::string toTipTap(const RawPage& page, double bodySize, const PdfImageOptions& images) {
    json content = json::array();

    if (page.kind == PageKind::Image && !page.imageId.empty()) {
        content.push_back(imageNode(images.urlFor(page.imageId), ""));
    } else {
        for (const auto& block : page.blocks) {
            if (block.isImage) {
                if (!block.imageId.empty()) content.push_back(imageNode(images.urlFor(block.imageId), ""));
                continue;
            }

            json node;
            const int level = headingLevel(block, bodySize);
            if (level > 0) node = {{"type", "heading"}, {"attrs", {{"level", level}}}};
            else           node = {{"type", "paragraph"}};

            json texts = json::array();
            for (const auto& run : block.runs) {
                if (run.text.empty()) continue;
                json text = {{"type", "text"}, {"text", run.text}};
                json marks = json::array();
                // Headings are bold already; marking them bold again is just noise.
                if (run.bold && level == 0) marks.push_back({{"type", "bold"}});
                if (run.italic)            marks.push_back({{"type", "italic"}});
                if (!marks.empty()) text["marks"] = marks;
                texts.push_back(std::move(text));
            }
            if (texts.empty()) continue;
            node["content"] = std::move(texts);
            content.push_back(std::move(node));
        }
    }

    // ProseMirror needs at least one block in a doc, so a blank page is one empty paragraph.
    if (content.empty()) content.push_back({{"type", "paragraph"}});

    json doc = {{"type", "doc"}, {"content", std::move(content)}};
    return doc.dump(-1, ' ', false, json::error_handler_t::replace);
}

// ---------------------------------------------------------------------------
// Figures: drawings made of lines and shapes. MuPDF reports embedded pictures as image
// blocks, but a diagram drawn with vector graphics only shows up as its stray label letters
// ("e:d e:"). So: draw the page small, rub out the text, and whatever ink is left is a drawing.
// ---------------------------------------------------------------------------

// Words of 3+ letters in the text that sits inside `r`. Figure labels are digits, single
// letters and fragments ("1 2 3", "d e:"); code listings and notes are full of real words.
int wordsInside(const fz_rect& r, const std::vector<Block>& blocks) {
    const fz_rect grown{r.x0 - 6, r.y0 - 6, r.x1 + 6, r.y1 + 6};
    int words = 0;
    for (const auto& block : blocks) {
        if (block.isImage || area(block.rect) <= 0) continue;
        if (area(fz_intersect_rect(block.rect, grown)) < area(block.rect) * 0.5f) continue;
        int letters = 0;
        for (char c : block.text() + " ") {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) { letters++; continue; }
            if (letters >= 3) words++;
            letters = 0;
        }
    }
    return words;
}

// Thin lines vanish when a page is drawn this small: at a quarter size a 0.25 pt line is a
// faint grey smudge. For this one drawing, turn off anti-aliasing and make every line at least
// a pixel wide, so any line shows up as solid ink. The settings go back afterwards, because the
// figures themselves are drawn with them.
struct DetectionDrawing {
    fz_context* ctx;
    int         aa;
    float       minLine;
    explicit DetectionDrawing(fz_context* c)
        : ctx(c), aa(fz_graphics_aa_level(c)), minLine(fz_graphics_min_line_width(c)) {
        fz_set_graphics_aa_level(ctx, 0);
        fz_set_graphics_min_line_width(ctx, 1.0f);
    }
    ~DetectionDrawing() {
        fz_set_graphics_aa_level(ctx, aa);
        fz_set_graphics_min_line_width(ctx, minLine);
    }
};

std::vector<fz_rect> findFigures(fz_context* ctx, fz_page* page, const fz_rect& bounds, const std::vector<Block>& blocks) {
    constexpr float ZOOM   = 0.35f;   // about 25 dpi: enough to see a drawing, cheap to render
    constexpr int   CELL   = 4;       // pixels per grid cell, about 11 pt
    constexpr int   DARK   = 160;     // lines and solid shapes
    constexpr int   LIGHT  = 235;     // also light grey fills: tiles, boxes, code listing shading
    constexpr int   REACH  = 2;       // cells: parts of one drawing closer than ~23 pt belong together

    fz_pixmap* raw = nullptr;
    {
        DetectionDrawing settings(ctx);   // restored at the end of this block, even if drawing fails
        raw = renderArea(ctx, page, bounds, ZOOM, true);
    }
    Pixmap drawn{ctx, raw};               // freed when findFigures returns
    fz_pixmap* gray = drawn.pix;
    const int w = fz_pixmap_width(ctx, gray), h = fz_pixmap_height(ctx, gray);
    const int ox = fz_pixmap_x(ctx, gray), oy = fz_pixmap_y(ctx, gray);
    const int stride = static_cast<int>(fz_pixmap_stride(ctx, gray));
    const unsigned char* pixels = fz_pixmap_samples(ctx, gray);
    if (w <= 0 || h <= 0) return {};

    // 2 = dark ink, 1 = light grey, 0 = paper
    std::vector<unsigned char> ink(static_cast<size_t>(w) * h, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const unsigned char v = pixels[y * stride + x];
            ink[y * w + x] = v < DARK ? 2 : v < LIGHT ? 1 : 0;
        }

    // Rub out the text, letter by letter (a block's outline can stretch across a drawing whose
    // labels belong to it), and the pictures that are handled already.
    const auto erase = [&](const fz_rect& r) {
        const int x0 = std::max(0, static_cast<int>(std::floor(r.x0 * ZOOM)) - ox - 1);
        const int y0 = std::max(0, static_cast<int>(std::floor(r.y0 * ZOOM)) - oy - 1);
        const int x1 = std::min(w, static_cast<int>(std::ceil(r.x1 * ZOOM)) - ox + 1);
        const int y1 = std::min(h, static_cast<int>(std::ceil(r.y1 * ZOOM)) - oy + 1);
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) ink[y * w + x] = 0;
    };
    for (const auto& block : blocks) {
        if (block.isImage) erase(block.rect);
        for (const auto& glyph : block.glyphs) erase(glyph);
    }

    // Coarse grid. A cell is dark with a couple of dark pixels in it, light when it's mostly
    // filled with grey (a fill, not a stray anti-aliased edge).
    const int gw = (w + CELL - 1) / CELL, gh = (h + CELL - 1) / CELL;
    std::vector<int> darkCount(static_cast<size_t>(gw) * gh, 0), lightCount(darkCount.size(), 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const int c = (y / CELL) * gw + x / CELL;
            if (ink[y * w + x] == 2) darkCount[c]++;
            else if (ink[y * w + x] == 1) lightCount[c]++;
        }
    std::vector<unsigned char> dark(darkCount.size()), light(darkCount.size());
    for (size_t c = 0; c < darkCount.size(); c++) {
        dark[c]  = darkCount[c] >= 2;
        light[c] = lightCount[c] >= CELL * CELL / 2;
    }

    // Group nearby inked cells into drawings (flood fill, allowing REACH cells of gap).
    std::vector<int> group(dark.size(), -1);
    std::vector<fz_rect> figures;
    const float pageArea = area(bounds);
    for (int start = 0; start < static_cast<int>(dark.size()); start++) {
        if ((!dark[start] && !light[start]) || group[start] != -1) continue;
        const int id = start;
        int minX = gw, minY = gh, maxX = -1, maxY = -1, cells = 0, darkCells = 0;
        std::vector<int> stack{start};
        group[start] = id;
        while (!stack.empty()) {
            const int c = stack.back(); stack.pop_back();
            const int cx = c % gw, cy = c / gw;
            minX = std::min(minX, cx); maxX = std::max(maxX, cx);
            minY = std::min(minY, cy); maxY = std::max(maxY, cy);
            cells++;
            if (dark[c]) darkCells++;
            for (int dy = -REACH; dy <= REACH; dy++)
                for (int dx = -REACH; dx <= REACH; dx++) {
                    const int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    const int n = ny * gw + nx;
                    if ((dark[n] || light[n]) && group[n] == -1) { group[n] = id; stack.push_back(n); }
                }
        }

        const fz_rect r{(ox + minX * CELL) / ZOOM, (oy + minY * CELL) / ZOOM,
                        (ox + (maxX + 1) * CELL) / ZOOM, (oy + (maxY + 1) * CELL) / ZOOM};
        // Rules under headings and lines between paragraphs are thin; real figures aren't.
        if (r.x1 - r.x0 < 60 || r.y1 - r.y0 < 30 || area(r) < pageArea * 0.02f || cells < 6) continue;

        if (darkCells >= 6) {
            // Lines and shapes. A frame around a sidebar or a note has dark ink only along its
            // edges: that's text in a box, not a figure, so the text inside stays text.
            int inside = 0;
            for (int cy = minY + 1; cy < maxY; cy++)
                for (int cx = minX + 1; cx < maxX; cx++)
                    if (dark[cy * gw + cx] && group[cy * gw + cx] == id) inside++;
            if (inside < 3) continue;
        } else {
            // Only light grey: grey tiles or boxes in a diagram, or the shading behind a code
            // listing or a note. A diagram's text is labels; a listing's is words.
            if (wordsInside(r, blocks) > 3) continue;
        }

        figures.push_back(r);
    }
    return figures;
}

// Puts the figures into the page's blocks: text and pictures that sit inside a figure (its
// labels) become part of it, and each figure goes where it appears in the reading order.
void placeFigures(std::vector<Block>& blocks, const std::vector<fz_rect>& figures) {
    for (fz_rect figure : figures) {
        const fz_rect grown{figure.x0 - 6, figure.y0 - 6, figure.x1 + 6, figure.y1 + 6};
        std::vector<Block> kept;
        for (auto& block : blocks) {
            const float blockArea = area(block.rect);
            const bool insideFigure = !block.isFigure && blockArea > 0 &&
                                      area(fz_intersect_rect(block.rect, grown)) >= blockArea * 0.5f;
            if (insideFigure) figure = fz_union_rect(figure, block.rect);   // labels sticking out a little
            else kept.push_back(std::move(block));
        }

        Block drawing;
        drawing.isImage = true;
        drawing.isFigure = true;
        drawing.rect = figure;
        auto at = std::find_if(kept.begin(), kept.end(), [&](const Block& b) { return b.rect.y0 >= figure.y0 - 1; });
        kept.insert(at, std::move(drawing));
        blocks = std::move(kept);
    }
}

bool isBlankPage(const RawPage& page) {
    if (page.blocks.empty()) return true;
    if (page.blocks.size() > 2) return false;
    for (const auto& block : page.blocks) {
        if (block.isImage) return false;
        std::string lower = block.text();
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (lower.find("intentionally left blank") == std::string::npos && lower.find("intentionally blank") == std::string::npos) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

class ImageBudget {
public:
    explicit ImageBudget(int maxImages) : left_(maxImages) {}
    bool allows() const { return left_ > 0 && bytes_ < MAX_IMAGE_BYTES; }
    void spend(size_t bytes) { left_--; bytes_ += bytes; }
    void refuse() { refused_++; }
    int  refused() const { return refused_; }
    bool outOfBytes() const { return bytes_ >= MAX_IMAGE_BYTES; }
private:
    int    left_;
    size_t bytes_ = 0;
    int    refused_ = 0;     // pictures and image pages left out
};

ExtractedImage render(fz_context* ctx, fz_page* page, fz_rect area, float zoom) {
    Pixmap pixmap{ctx, renderArea(ctx, page, area, zoom)};
    Buffer buffer{ctx, encodePng(ctx, pixmap.pix)};
    unsigned char* data = nullptr;
    const size_t size = fz_buffer_storage(ctx, buffer.buf, &data);
    return ExtractedImage{"", std::string(reinterpret_cast<const char*>(data), size), "image/png"};
}

void readOutline(fz_context* ctx, fz_document* doc, fz_outline* item, int level, int pageCount, std::vector<TocEntry>& toc) {
    for (; item && toc.size() < MAX_TOC; item = item->next) {
        const int page = outlinePage(ctx, doc, item);
        const std::string title = item->title ? cleanUtf8(item->title, MAX_TOC_TITLE) : "";
        if (page >= 0 && page < pageCount && !title.empty()) {
            toc.push_back(TocEntry{title, level, page + 1});   // PDF page number for now, remapped later
        }
        if (item->down) readOutline(ctx, doc, item->down, level + 1, pageCount, toc);
    }
}

}  // namespace

ExtractedPdf extractPdf(const std::string& path, const PdfLimits& limits, const PdfImageOptions& images,
                        const PdfProgress& progress) {
    const auto started = std::chrono::steady_clock::now();

    Context mu;
    mu.ctx = fz_new_context(nullptr, nullptr, STORE_BYTES);
    if (!mu.ctx) throw std::runtime_error("Could not start MuPDF (out of memory)");
    mu.doc = openDocument(mu.ctx, path.c_str());

    if (isLocked(mu.ctx, mu.doc)) throw PdfError("The PDF is password protected");

    const int totalPages = countPages(mu.ctx, mu.doc);
    if (totalPages <= 0) throw PdfError("The PDF has no pages");
    // Longer than the limit: import the first maxPages pages and say so, rather than failing.
    const int pageCount = std::min(totalPages, limits.maxPages);

    ExtractedPdf result;

    // The outline first: whether it exists decides if printed contents pages can be dropped.
    Outline outline{mu.ctx, loadOutline(mu.ctx, mu.doc)};
    readOutline(mu.ctx, mu.doc, outline.root, 0, pageCount, result.toc);
    const bool hasOutline = !result.toc.empty();

    // Pass 1: read each page, decide what kind it is, render its pictures.
    std::vector<RawPage> raw;
    raw.reserve(pageCount);
    std::map<long, size_t> sizes;
    ImageBudget budget(limits.maxImages);
    bool previousWasToc = false;

    for (int i = 0; i < pageCount; i++) {
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (elapsed > std::chrono::seconds(limits.timeoutSeconds)) {
            throw PdfError("Processing the PDF took longer than " + std::to_string(limits.timeoutSeconds) + " seconds");
        }

        Page page{mu.ctx, loadPage(mu.ctx, mu.doc, i)};
        const fz_rect bounds = pageBounds(mu.ctx, page.page);
        TextPage text{mu.ctx, loadText(mu.ctx, page.page)};

        RawPage current = readPage(mu.ctx, text.page, bounds, sizes);
        current.kind = classify(statsOf(current), i, pageCount, hasOutline, previousWasToc);
        previousWasToc = current.kind == PageKind::Skip;

        const float pageWidth = std::max(1.0f, bounds.x1 - bounds.x0);
        const float pageZoom  = std::min(2.0f, PAGE_WIDTH_PX / pageWidth);

        if (current.kind == PageKind::Cover) {
            result.cover = render(mu.ctx, page.page, bounds, pageZoom);
        } else if (current.kind == PageKind::Image) {
            if (budget.allows()) {
                ExtractedImage image = render(mu.ctx, page.page, bounds, pageZoom);
                image.publicId = images.prefix + "/p" + std::to_string(i + 1);
                budget.spend(image.data.size());
                current.imageId = image.publicId;
                result.images.push_back(std::move(image));
            } else {
                budget.refuse();
                current.kind = PageKind::Text;   // out of image budget: keep whatever text it has
            }
        } else if (current.kind == PageKind::Text) {
            placeFigures(current.blocks, findFigures(mu.ctx, page.page, bounds, current.blocks));
            for (auto& block : current.blocks) std::vector<fz_rect>().swap(block.glyphs);   // not needed any more

            int n = 0;
            for (auto& block : current.blocks) {
                if (!block.isImage) continue;
                const float w = block.rect.x1 - block.rect.x0, h = block.rect.y1 - block.rect.y0;
                // Skip bullets, icons, rules and other decorations (figures are big enough already).
                if (!block.isFigure && (w < 40 || h < 40 || area(block.rect) < area(bounds) * 0.01f)) continue;
                if (!budget.allows()) { budget.refuse(); continue; }

                const float zoom = std::min(2.0f, IMAGE_SIDE_PX / std::max(w, h));
                ExtractedImage image = render(mu.ctx, page.page, block.rect, zoom);
                image.publicId = images.prefix + "/p" + std::to_string(i + 1) + "-" + std::to_string(++n);
                budget.spend(image.data.size());
                block.imageId = image.publicId;
                result.images.push_back(std::move(image));
            }
        }

        if (current.kind != PageKind::Text) current.blocks.clear();   // nothing else needed from them
        raw.push_back(std::move(current));
        if (progress) progress(i + 1, pageCount);
    }

    removeRunningHeaders(raw);

    // Pages with nothing left on them: blank in the printed book, or only a header and a page number.
    for (auto& page : raw) {
        if (page.kind == PageKind::Text && isBlankPage(page)) page.kind = PageKind::Blank;
    }

    // Pass 2: the body text size is known now, so build each page's TipTap document.
    double bodySize = 0;
    size_t most = 0;
    for (const auto& [halfPoints, count] : sizes) {
        if (count > most) { most = count; bodySize = halfPoints / 2.0; }
    }

    // Dropped pages (cover, printed contents, blank pages) shift the rest up: PDF page -> book page.
    std::vector<int> bookPage(pageCount, 0);
    int next = 0;
    int blankPages = 0;
    for (int i = 0; i < pageCount; i++) {
        if (raw[i].kind == PageKind::Cover || raw[i].kind == PageKind::Skip || raw[i].kind == PageKind::Blank) {
            if (raw[i].kind == PageKind::Skip)  result.skippedPages++;
            if (raw[i].kind == PageKind::Blank) blankPages++;
            continue;
        }
        bookPage[i] = ++next;
        result.pages.push_back(PageContent{next, toTipTap(raw[i], bodySize, images)});
        raw[i] = RawPage();   // free as we go
    }
    if (result.pages.empty()) throw PdfError("The PDF has no readable pages");

    // Things the user should know were left out. The cover is reported by the task handler,
    // which knows whether it was used.
    if (totalPages > pageCount) {
        result.warnings.push_back("Only the first " + std::to_string(pageCount) + " of " + std::to_string(totalPages) +
            " pages were imported: the limit is " + std::to_string(limits.maxPages) + " pages per book");
    }
    if (budget.refused() > 0) {
        result.warnings.push_back(std::to_string(budget.refused()) + " picture" + (budget.refused() == 1 ? " was" : "s were") +
            " left out: " + (budget.outOfBytes() ? "the book's pictures reached the 80 MB limit"
                                                 : "the limit is " + std::to_string(limits.maxImages) + " per book"));
    }
    if (blankPages > 0) {
        result.warnings.push_back(std::to_string(blankPages) + " blank page" + (blankPages == 1 ? " was" : "s were") + " removed");
    }
    if (result.skippedPages > 0) {
        result.warnings.push_back(std::to_string(result.skippedPages) + " printed table of contents page" +
            (result.skippedPages == 1 ? " was" : "s were") + " removed; the table of contents comes from the PDF's outline instead");
    }

    // An entry pointing at a dropped page goes to the next page that was kept.
    for (auto& entry : result.toc) {
        int pdfIndex = entry.pageNumber - 1;
        while (pdfIndex < pageCount && bookPage[pdfIndex] == 0) pdfIndex++;
        entry.pageNumber = pdfIndex < pageCount ? bookPage[pdfIndex] : next;
    }

    return result;
}

PdfExtractor::PdfExtractor(PdfLimits limits) : limits_(limits), thread_("PdfExtract") {
    thread_.run();
}

drogon::Task<ExtractedPdf> PdfExtractor::extract(std::string path, PdfImageOptions images, PdfProgress progress) {
    trantor::EventLoop* origin = trantor::EventLoop::getEventLoopOfCurrentThread();

    // Only one thread does extraction, so a second PDF waits for the first to finish.
    ExtractedPdf result;
    std::exception_ptr error;
    co_await drogon::queueInLoopCoro(thread_.getLoop(), [&]() {
        try {
            result = extractPdf(path, limits_, images, progress);
        } catch (...) {
            error = std::current_exception();
        }
    });

    if (origin) co_await drogon::switchThreadCoro(origin);
    if (error) std::rethrow_exception(error);
    co_return result;
}
