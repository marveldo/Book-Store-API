#include "PdfExtractor.hpp"
#include <mupdf/fitz.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_map>

using nlohmann::json;

namespace {

// ---------------------------------------------------------------------------
// MuPDF reports errors with setjmp/longjmp (fz_try / fz_catch), not C++ exceptions.
// Each helper below wraps exactly one MuPDF call that can fail and turns the
// failure into a PdfError. Keep them free of C++ objects with destructors:
// a longjmp skips destructors. Variables set inside fz_try are `volatile`
// because their value after a longjmp is otherwise undefined.
// ---------------------------------------------------------------------------

constexpr size_t STORE_BYTES   = 32 * 1024 * 1024;  // MuPDF's cache; its default is 256 MB
constexpr size_t MAX_TOC       = 2000;
constexpr size_t MAX_TOC_TITLE = 500;

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

fz_stext_page* loadText(fz_context* ctx, fz_document* doc, int index) {
    fz_stext_page* volatile text = nullptr;
    fz_stext_options options{};
    fz_try(ctx) text = fz_new_stext_page_from_page_number(ctx, doc, index, &options);
    fz_catch(ctx) fail(ctx, "Could not read a page of the PDF");
    return text;
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

// Frees MuPDF objects when extractPdf returns or throws.
struct Context {
    fz_context*  ctx = nullptr;
    fz_document* doc = nullptr;
    ~Context() {
        if (doc) fz_drop_document(ctx, doc);
        if (ctx) fz_drop_context(ctx);
    }
};

struct TextPage {
    fz_context*     ctx;
    fz_stext_page*  page;
    ~TextPage() { fz_drop_stext_page(ctx, page); }
};

struct Outline {
    fz_context* ctx;
    fz_outline* root;
    ~Outline() { if (root) fz_drop_outline(ctx, root); }
};

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
    // Trim spaces at both ends.
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    return out.substr(first, out.find_last_not_of(' ') - first + 1);
}

struct Run {
    std::string text;
    bool bold;
    bool italic;
};

struct Block {
    std::vector<Run> runs;
    double sizeSum = 0;
    size_t chars   = 0;

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

using RawPage = std::vector<Block>;

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

// Page numbers printed in headers/footers ("12") aren't content.
bool isPageNumber(const std::string& text) {
    return !text.empty() && text.size() <= 4 && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

RawPage readPage(fz_context* ctx, fz_stext_page* page, std::map<long, size_t>& sizes) {
    RawPage blocks;
    std::unordered_map<fz_font*, FontStyle> fonts;

    for (fz_stext_block* b = page->first_block; b; b = b->next) {
        if (b->type != FZ_STEXT_BLOCK_TEXT) continue;   // images come in a later version

        Block block;
        bool lineBreak = false;
        for (fz_stext_line* line = b->u.t.first_line; line; line = line->next) {
            if (!block.runs.empty()) lineBreak = true;

            for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
                int c = ch->c;
                if (!keep(c)) continue;
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0) {
                    block.addSpace();
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
                sizes[std::lround(ch->size * 2)]++;
            }
        }

        block.trim();
        if (block.chars == 0 || isPageNumber(block.text())) continue;
        blocks.push_back(std::move(block));
    }
    return blocks;
}

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

// TipTap / ProseMirror document: {"type":"doc","content":[ paragraphs and headings ]}
std::string toTipTap(const RawPage& blocks, double bodySize) {
    json content = json::array();

    for (const auto& block : blocks) {
        json node;
        const int level = headingLevel(block, bodySize);
        if (level > 0) {
            node = {{"type", "heading"}, {"attrs", {{"level", level}}}};
        } else {
            node = {{"type", "paragraph"}};
        }

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

    // ProseMirror needs at least one block in a doc, so a blank page is one empty paragraph.
    if (content.empty()) content.push_back({{"type", "paragraph"}});

    json doc = {{"type", "doc"}, {"content", std::move(content)}};
    return doc.dump(-1, ' ', false, json::error_handler_t::replace);
}

void readOutline(fz_context* ctx, fz_document* doc, fz_outline* item, int level, int pageCount, std::vector<TocEntry>& toc) {
    for (; item && toc.size() < MAX_TOC; item = item->next) {
        const int page = outlinePage(ctx, doc, item);
        const std::string title = item->title ? cleanUtf8(item->title, MAX_TOC_TITLE) : "";
        if (page >= 0 && page < pageCount && !title.empty()) {
            toc.push_back(TocEntry{title, level, page + 1});
        }
        if (item->down) readOutline(ctx, doc, item->down, level + 1, pageCount, toc);
    }
}

}  // namespace

ExtractedPdf extractPdf(const std::string& path, const PdfLimits& limits, const PdfProgress& progress) {
    const auto started = std::chrono::steady_clock::now();

    Context mu;
    mu.ctx = fz_new_context(nullptr, nullptr, STORE_BYTES);
    if (!mu.ctx) throw std::runtime_error("Could not start MuPDF (out of memory)");
    mu.doc = openDocument(mu.ctx, path.c_str());

    if (isLocked(mu.ctx, mu.doc)) throw PdfError("The PDF is password protected");

    const int pageCount = countPages(mu.ctx, mu.doc);
    if (pageCount <= 0) throw PdfError("The PDF has no pages");
    if (pageCount > limits.maxPages) {
        throw PdfError("The PDF has " + std::to_string(pageCount) + " pages; the limit is " + std::to_string(limits.maxPages));
    }

    // Pass 1: read every page's text and count font sizes.
    std::vector<RawPage> raw;
    raw.reserve(pageCount);
    std::map<long, size_t> sizes;

    for (int i = 0; i < pageCount; i++) {
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (elapsed > std::chrono::seconds(limits.timeoutSeconds)) {
            throw PdfError("Processing the PDF took longer than " + std::to_string(limits.timeoutSeconds) + " seconds");
        }

        TextPage text{mu.ctx, loadText(mu.ctx, mu.doc, i)};
        raw.push_back(readPage(mu.ctx, text.page, sizes));
        if (progress) progress(i + 1, pageCount);
    }

    // Pass 2: now that the body size is known, turn each page into a TipTap document.
    double bodySize = 0;
    size_t most = 0;
    for (const auto& [halfPoints, count] : sizes) {
        if (count > most) { most = count; bodySize = halfPoints / 2.0; }
    }

    ExtractedPdf result;
    result.pages.reserve(pageCount);
    for (int i = 0; i < pageCount; i++) {
        result.pages.push_back(PageContent{i + 1, toTipTap(raw[i], bodySize)});
        RawPage().swap(raw[i]);   // free as we go
    }

    Outline outline{mu.ctx, loadOutline(mu.ctx, mu.doc)};
    readOutline(mu.ctx, mu.doc, outline.root, 0, pageCount, result.toc);

    return result;
}

PdfExtractor::PdfExtractor(PdfLimits limits) : limits_(limits), thread_("PdfExtract") {
    thread_.run();
}

drogon::Task<ExtractedPdf> PdfExtractor::extract(std::string path, PdfProgress progress) {
    trantor::EventLoop* origin = trantor::EventLoop::getEventLoopOfCurrentThread();

    // Only one thread does extraction, so a second PDF waits for the first to finish.
    ExtractedPdf result;
    std::exception_ptr error;
    co_await drogon::queueInLoopCoro(thread_.getLoop(), [&]() {
        try {
            result = extractPdf(path, limits_, progress);
        } catch (...) {
            error = std::current_exception();
        }
    });

    if (origin) co_await drogon::switchThreadCoro(origin);
    if (error) std::rethrow_exception(error);
    co_return result;
}
