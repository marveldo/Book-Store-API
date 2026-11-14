#pragma once
#include <string>

inline const std::string USER_CREATED = "user.created";
inline const std::string PDF_UPLOADED = "pdf.uploaded";
inline const std::string PAYMENT_VERIFY_DUE = "payment.verify_due";
inline const std::string PAYMENT_REFUND_DUE = "payment.refund_due";

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

struct PaymentVerifyDue {
    std::string reference;
};

struct PaymentRefundDue {
    std::string reference;
};
