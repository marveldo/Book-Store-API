CREATE TABLE book_comments (
    id         UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    book_id    UUID        NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    user_id    UUID        NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    content    TEXT        NOT NULL CHECK (length(content) > 0),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX book_comments_book_id_created_at_idx ON book_comments (book_id, created_at DESC);

CREATE TRIGGER book_comments_set_updated_at
    BEFORE UPDATE ON book_comments
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
