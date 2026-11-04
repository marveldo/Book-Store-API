ALTER TABLE user_settings ALTER COLUMN user_id SET NOT NULL;
ALTER TABLE user_settings ADD CONSTRAINT user_settings_user_id_key UNIQUE (user_id);
