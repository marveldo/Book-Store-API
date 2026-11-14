ALTER TABLE payments
    ADD COLUMN reference     TEXT UNIQUE,
    ADD COLUMN checkout_url  TEXT,
    ADD COLUMN status        TEXT NOT NULL DEFAULT 'success'
                             CHECK (status IN ('pending', 'success', 'failed', 'abandoned',
                                               'refund_pending', 'refunding', 'refunded', 'refund_failed')),
    ADD COLUMN currency      TEXT NOT NULL DEFAULT 'NGN',
    ADD COLUMN created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
    ADD COLUMN paid_at       TIMESTAMPTZ,
    ADD COLUMN next_check_at TIMESTAMPTZ,
    ADD COLUMN check_count   INT NOT NULL DEFAULT 0;

CREATE INDEX payments_due ON payments (next_check_at) WHERE status = 'pending';
CREATE UNIQUE INDEX payments_paid_once ON payments (order_id) WHERE status = 'success';
