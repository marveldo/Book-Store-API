#pragma once
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThread.h>
#include "services/book-pages/BookPages.hpp"
#include "input/BookTocInputs.hpp"
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

// A problem with the PDF itself (corrupt, password protected, too many pages, too slow).
// Trying again won't fix it, so the task handler turns this into a PermanentTaskError.
struct PdfError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct PdfLimits {
    int maxPages;
    int timeoutSeconds;
};

struct ExtractedPdf {
    std::vector<PageContent> pages;   // content is a TipTap / ProseMirror JSON document
    std::vector<TocEntry>    toc;
};

// Called after each page with (pages done, total pages). Runs on the extraction thread.
using PdfProgress = std::function<void(int done, int total)>;

// Blocking. Reads the PDF at `path` with MuPDF.
ExtractedPdf extractPdf(const std::string& path, const PdfLimits& limits, const PdfProgress& progress);

// Runs extractPdf on its own thread, one PDF at a time, so it never blocks
// the task queue loop or Drogon's IO threads.
class PdfExtractor {
public:
    explicit PdfExtractor(PdfLimits limits);

    PdfExtractor(const PdfExtractor&) = delete;
    PdfExtractor& operator=(const PdfExtractor&) = delete;

    drogon::Task<ExtractedPdf> extract(std::string path, PdfProgress progress);

private:
    PdfLimits limits_;
    trantor::EventLoopThread thread_;
};
