CREATE TABLE books (
    id             UUID  PRIMARY KEY DEFAULT gen_random_uuid(),
    title          VARCHAR(255)  NOT NULL,
    author         VARCHAR(255)  NOT NULL,
    isbn           VARCHAR(13)   UNIQUE,            
    description    TEXT,
    published_year SMALLINT,
    price          NUMERIC(10,2) NOT NULL DEFAULT 0 CHECK (price >= 0),
    created_at     TIMESTAMPTZ   NOT NULL DEFAULT NOW(),
    updated_at     TIMESTAMPTZ   NOT NULL DEFAULT NOW()
);

CREATE INDEX books_title_idx  ON books (title);
CREATE INDEX books_author_idx ON books (author);

CREATE TRIGGER books_set_updated_at
    BEFORE UPDATE ON books
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
