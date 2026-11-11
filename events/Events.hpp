#pragma once
#include <string>

inline const std::string USER_CREATED = "user.created";
inline const std::string PDF_UPLOADED = "pdf.uploaded";

struct UserCreated {
    std::string id;
    std::string email;
    std::string username;
};

struct PdfUploaded {
    std::string jobId;
    std::string bookId;
    std::string path;
};
