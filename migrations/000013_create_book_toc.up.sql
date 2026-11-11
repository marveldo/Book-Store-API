CREATE TABLE book_toc (
    id          UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
    book_id     UUID        NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    title       TEXT        NOT NULL,
    level       INTEGER     NOT NULL DEFAULT 0 CHECK (level >= 0),
    page_number INTEGER     NOT NULL CHECK (page_number > 0),
    position    INTEGER     NOT NULL CHECK (position >= 0),
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (book_id, position)
);
