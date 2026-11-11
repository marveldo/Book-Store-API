ALTER TABLE books ADD COLUMN owner_id UUID REFERENCES users(id) ON DELETE SET NULL;

CREATE INDEX books_owner_id_idx ON books (owner_id);
