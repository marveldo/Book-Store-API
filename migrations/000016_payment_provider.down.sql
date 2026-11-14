DROP INDEX IF EXISTS payments_paid_once;
DROP INDEX IF EXISTS payments_due;

ALTER TABLE payments
    DROP COLUMN check_count,
    DROP COLUMN next_check_at,
    DROP COLUMN paid_at,
    DROP COLUMN created_at,
    DROP COLUMN currency,
    DROP COLUMN status,
    DROP COLUMN checkout_url,
    DROP COLUMN reference;
