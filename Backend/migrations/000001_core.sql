-- 000001_core.sql（核心数据库结构）：建立第一版长期权威业务表。
CREATE TABLE IF NOT EXISTS player_profiles (
    player_id TEXT PRIMARY KEY,
    game_id TEXT NOT NULL,
    display_name TEXT NOT NULL,
    data_version INTEGER NOT NULL DEFAULT 1,
    revision BIGINT NOT NULL DEFAULT 1,
    tutorial_completed BOOLEAN NOT NULL DEFAULT FALSE,
    default_world_id TEXT NOT NULL DEFAULT '',
    owned_character_ids JSONB NOT NULL DEFAULT '[]'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS match_results (
    match_id TEXT PRIMARY KEY,
    result_id TEXT NOT NULL UNIQUE,
    arena_mode_id TEXT NOT NULL,
    game_server_id TEXT NOT NULL,
    winning_team_id TEXT,
    started_at TIMESTAMPTZ,
    ended_at TIMESTAMPTZ,
    payload JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
