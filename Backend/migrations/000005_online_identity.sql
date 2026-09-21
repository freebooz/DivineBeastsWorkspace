-- Online身份领域独占：账户凭据、认证会话和刷新消费历史，不创建或修改玩家资料。
-- 由受控迁移工具在已批准隔离库按版本登记后执行；服务启动不可自动应用。
CREATE TABLE online_identity_accounts (
    game_id TEXT NOT NULL CHECK (octet_length(game_id) BETWEEN 1 AND 128),
    account_name TEXT NOT NULL CHECK (octet_length(account_name) BETWEEN 1 AND 128),
    player_id TEXT NOT NULL UNIQUE,
    password_hash BYTEA NOT NULL,
    disabled BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (game_id, account_name),
    UNIQUE (game_id, player_id)
);

CREATE TABLE online_identity_sessions (
    session_id TEXT PRIMARY KEY,
    game_id TEXT NOT NULL,
    player_id TEXT NOT NULL,
    device_id TEXT NOT NULL DEFAULT '',
    access_digest TEXT NOT NULL UNIQUE CHECK (access_digest ~ '^[0-9a-f]{64}$'),
    refresh_digest TEXT NOT NULL UNIQUE CHECK (refresh_digest ~ '^[0-9a-f]{64}$'),
    access_expires_at TIMESTAMPTZ NOT NULL,
    refresh_expires_at TIMESTAMPTZ NOT NULL,
    revoked BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    FOREIGN KEY (game_id, player_id) REFERENCES online_identity_accounts (game_id, player_id),
    CHECK (access_expires_at <= refresh_expires_at)
);

-- 包含当前和历史所有刷新摘要；不得在轮换或退出时删除，否则旧凭据无法检测重放/幂等撤销。
-- M0不自动清理，后续清理策略须同时保留承诺的退出幂等期限和安全审计边界。
CREATE TABLE online_identity_refresh_credentials (
    digest TEXT PRIMARY KEY CHECK (digest ~ '^[0-9a-f]{64}$'),
    session_id TEXT NOT NULL REFERENCES online_identity_sessions (session_id),
    consumed_at TIMESTAMPTZ NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX online_identity_refresh_session_idx ON online_identity_refresh_credentials (session_id);
CREATE INDEX online_identity_session_player_idx ON online_identity_sessions (game_id, player_id);
