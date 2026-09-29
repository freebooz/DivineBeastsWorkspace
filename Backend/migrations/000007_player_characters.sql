-- 000007：持久角色与权威角色选择。
-- 本迁移只新增角色领域数据，不承载实时战斗状态；实时生命值、技能冷却等仍由UE Dedicated Server权威管理。

ALTER TABLE player_profiles
    ADD COLUMN selected_character_id TEXT NOT NULL DEFAULT '';

CREATE TABLE player_characters (
    character_id TEXT PRIMARY KEY,
    player_id TEXT NOT NULL REFERENCES player_profiles(player_id) ON DELETE CASCADE,
    creation_request_id TEXT NOT NULL CHECK (char_length(creation_request_id) BETWEEN 1 AND 128),
    canonical_request BYTEA NOT NULL CHECK (octet_length(canonical_request) > 0),
    hero_definition_id TEXT NOT NULL CHECK (char_length(hero_definition_id) BETWEEN 1 AND 256),
    character_name TEXT NOT NULL CHECK (char_length(character_name) BETWEEN 1 AND 24),
    character_revision BIGINT NOT NULL DEFAULT 1 CHECK (character_revision >= 1),
    onboarding_state TEXT NOT NULL DEFAULT 'TutorialRequired'
        CHECK (onboarding_state IN ('New','TutorialRequired','TutorialInProgress','OnboardingComplete')),
    status TEXT NOT NULL DEFAULT 'Active'
        CHECK (status IN ('Active','Disabled')),
    appearance_profile_id TEXT NOT NULL DEFAULT '',
    appearance_selection JSONB NOT NULL DEFAULT '{}'::jsonb
        CHECK (jsonb_typeof(appearance_selection) = 'object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (player_id, creation_request_id)
);

CREATE UNIQUE INDEX uq_player_characters_player_name_ci
    ON player_characters (player_id, lower(character_name));

CREATE INDEX idx_player_characters_player_created
    ON player_characters (player_id, created_at, character_id);

CREATE TABLE player_character_selection_idempotency (
    player_id TEXT NOT NULL REFERENCES player_profiles(player_id) ON DELETE CASCADE,
    selection_request_id TEXT NOT NULL CHECK (char_length(selection_request_id) BETWEEN 1 AND 128),
    canonical_request BYTEA NOT NULL CHECK (octet_length(canonical_request) > 0),
    response_snapshot JSONB NOT NULL CHECK (jsonb_typeof(response_snapshot) = 'object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (player_id, selection_request_id)
);

COMMENT ON TABLE player_characters IS '玩家账号下的持久角色；不保存实时战斗状态';
COMMENT ON COLUMN player_characters.creation_request_id IS '玩家作用域创建幂等键；同键同内容重放原角色';
COMMENT ON COLUMN player_characters.canonical_request IS '规范化创建请求快照，用于拒绝同键异内容';
COMMENT ON COLUMN player_profiles.selected_character_id IS '最近一次通过服务端权威验证的持久角色ID；空字符串表示尚未选择';
COMMENT ON TABLE player_character_selection_idempotency IS '角色选择持久幂等结果；与player_profiles selected_character_id更新同事务提交';
