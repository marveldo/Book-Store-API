DROP INDEX IF EXISTS books_owner_id_idx;

ALTER TABLE books DROP COLUMN IF EXISTS owner_id;
