#include "RegisterEvents.hpp"
#include "Events.hpp"

void RegisterEvents(EventBus* bus, TaskQueue* queue) {
    bus->Subscribe<UserCreated>(USER_CREATED, [queue](UserCreated event) {
        queue->enqueue("send_email", {
            {"to", event.email},
            {"subject", "Welcome to Mbooks"},
            {"template", "welcome.html"},
            {"data", {{"username", event.username}}}
        });
    });

    bus->Subscribe<UserCreated>(USER_CREATED, [queue](UserCreated event) {
        queue->enqueue("create_user_settings", {{"user_id", event.id}});
    });

    bus->Subscribe<PdfUploaded>(PDF_UPLOADED, [queue](PdfUploaded event) {
        queue->enqueue("extract_pdf", {
            {"job_id", event.jobId},
            {"book_id", event.bookId},
            {"path", event.path}
        });
    });

    bus->Subscribe<PaymentVerifyDue>(PAYMENT_VERIFY_DUE, [queue](PaymentVerifyDue event) {
        queue->enqueue("verify_payment", {{"reference", event.reference}});
    });

    bus->Subscribe<PaymentRefundDue>(PAYMENT_REFUND_DUE, [queue](PaymentRefundDue event) {
        queue->enqueue("refund_payment", {{"reference", event.reference}});
    });
}
