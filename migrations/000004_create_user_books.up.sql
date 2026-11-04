
CREATE TABLE user_books (
    user_id   UUID      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    book_id   UUID     NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    acquired_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, book_id)              
);


CREATE INDEX user_books_book_id_idx ON user_books (book_id);

