CREATE TABLE user_settings (
    id                  UUID     PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id             UUID     REFERENCES users(id) ON DELETE CASCADE,
    theme               VARCHAR(20) NOT NULL DEFAULT 'light',
    language            VARCHAR(10) NOT NULL DEFAULT 'en',
    timezone            VARCHAR(64) NOT NULL DEFAULT 'UTC',
    email_notifications BOOLEAN     NOT NULL DEFAULT TRUE,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at          TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT user_settings_theme_check CHECK (theme IN ('light', 'dark', 'system'))
);

CREATE TRIGGER user_settings_set_updated_at
    BEFORE UPDATE ON user_settings
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
