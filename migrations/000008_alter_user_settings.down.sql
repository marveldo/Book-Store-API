ALTER TABLE user_settings DROP CONSTRAINT user_settings_user_id_key;
ALTER TABLE user_settings ALTER COLUMN user_id DROP NOT NULL;