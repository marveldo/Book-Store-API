#pragma once
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThread.h>
#include "services/book-pages/BookPages.hpp"
#include "input/BookTocInputs.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// A problem with the PDF itself (corrupt, password protected, too many pages, too slow).
// Trying again won't fix it, so the task handler turns this into a PermanentTaskError.
struct PdfError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct PdfLimits {
    int maxPages;           // longer PDFs: only the first maxPages pages are imported
    int timeoutSeconds;
    int maxImages;          // pictures + rendered pages per book; later ones are left out
};

// What to call the images and how their URLs look. Comes from the task handler, so
// the extractor doesn't need to know about Cloudinary or the settings.
struct PdfImageOptions {
    std::string prefix;                                         // e.g. "books/<book>/<job>"
    std::function<std::string(const std::string&)> urlFor;      // public id -> URL used in the page
};

// A rendered image waiting to be uploaded. Its URL is already in a page's content.
struct ExtractedImage {
    std::string publicId;   // prefix + "/p12-1"
    std::string data;       // the encoded file
    std::string mime;       // image/png
};

struct ExtractedPdf {
    std::vector<PageContent>   pages;    // content is a TipTap / ProseMirror JSON document
    std::vector<TocEntry>      toc;      // page numbers already match `pages`
    std::vector<ExtractedImage> images;  // upload these before saving the pages
    std::optional<ExtractedImage> cover; // page 1, when it's a cover (no public id yet)
    int skippedPages = 0;                // printed table of contents pages left out
    std::vector<std::string> warnings;   // what was left out, for the user
};

// Called after each page with (pages done, total pages). Runs on the extraction thread.
using PdfProgress = std::function<void(int done, int total)>;

// Blocking. Reads the PDF at `path` with MuPDF.
ExtractedPdf extractPdf(const std::string& path, const PdfLimits& limits, const PdfImageOptions& images,
                        const PdfProgress& progress);

// Runs extractPdf on its own thread, one PDF at a time, so it never blocks
// the task queue loop or Drogon's IO threads.
class PdfExtractor {
public:
    explicit PdfExtractor(PdfLimits limits);

    PdfExtractor(const PdfExtractor&) = delete;
    PdfExtractor& operator=(const PdfExtractor&) = delete;

    drogon::Task<ExtractedPdf> extract(std::string path, PdfImageOptions images, PdfProgress progress);

private:
    PdfLimits limits_;
    trantor::EventLoopThread thread_;
};
