CREATE TABLE orders (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id       UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    book_id       UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    quantity      INT NOT NULL CHECK (quantity > 0),
    total_price   NUMERIC(10,2) NOT NULL CHECK (total_price >= 0)
)