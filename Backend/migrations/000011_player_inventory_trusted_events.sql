-- 000011：服务端已确认背包事件的持久证据日志；需先有000003玩家准入表、000010背包幂等授予和000002发件箱。
-- 只由经过内部身份认证、真实世界连接租约校验的可信入口写入。
-- SourceEventID必须由权威游戏服产生；事件表持久保存原始期望修订号，防止重试改变事件身份。
-- 收到事件不等于物品授予/消耗成功：最终以player_inventory_operations和同事务Outbox为准。

CREATE TABLE player_inventory_trusted_events (
    source_event_id TEXT PRIMARY KEY CHECK (char_length(source_event_id) BETWEEN 1 AND 256),
    event_kind TEXT NOT NULL CHECK (event_kind IN ('grant','consume')),
    game_id TEXT NOT NULL CHECK (char_length(game_id) BETWEEN 1 AND 256),
    player_id TEXT NOT NULL REFERENCES player_profiles(player_id) ON DELETE RESTRICT,
    game_server_id TEXT NOT NULL,
    server_boot_id TEXT NOT NULL,
    session_epoch BIGINT NOT NULL CHECK (session_epoch >= 1),
    connection_id TEXT NOT NULL CHECK (char_length(connection_id) BETWEEN 1 AND 256),
    expected_revision BIGINT NOT NULL CHECK (expected_revision >= 1),
    item_definition_id TEXT NOT NULL CHECK (char_length(item_definition_id) BETWEEN 1 AND 256),
    item_instance_id TEXT NOT NULL DEFAULT '',
    quantity INTEGER NOT NULL CHECK (quantity >= 1 AND quantity <= 1000000),
    reason_code TEXT NOT NULL CHECK (reason_code IN ('pickup','quest_reward','scripted_reward','item_use')),
    canonical_event JSONB NOT NULL CHECK (jsonb_typeof(canonical_event) = 'object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT inventory_trusted_event_type_check CHECK (
      (event_kind='grant' AND item_instance_id='' AND reason_code IN ('pickup','quest_reward','scripted_reward'))
      OR (event_kind='consume' AND char_length(item_instance_id) BETWEEN 1 AND 256 AND reason_code='item_use')
    )
);

CREATE INDEX idx_inventory_trusted_events_player
    ON player_inventory_trusted_events(game_id,player_id,created_at DESC);

COMMENT ON TABLE player_inventory_trusted_events IS
    '可信游戏服确认事件审计日志，保存原始事件/租约/请求快照；本表不是向客户端发放物品的公共接口';
COMMENT ON COLUMN player_inventory_trusted_events.canonical_event IS
    '经准入租约校验并持久化的事件原文，重复SourceEventID改变字段必须拒绝';
REVOKE ALL ON player_inventory_trusted_events FROM PUBLIC;
