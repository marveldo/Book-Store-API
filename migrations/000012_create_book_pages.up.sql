CREATE TABLE book_pages (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    book_id     UUID        NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    page_number INTEGER     NOT NULL CHECK (page_number > 0),
    content     TEXT        NOT NULL,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (book_id, page_number)
);

CREATE TRIGGER book_pages_set_updated_at
    BEFORE UPDATE ON book_pages
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
