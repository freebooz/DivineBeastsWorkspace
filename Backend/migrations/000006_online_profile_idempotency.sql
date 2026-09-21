-- 000006：玩家资料领域的持久幂等结果；须由授权迁移入口显式应用，本脚本不自动启动或清空数据库。
-- 资料更新与本表插入属于同一事务；只有成功提交的完整结果可见，不预先提交“处理中”占位行。
-- 记录不设置短期TTL：不能在客户端可能重试时丢弃结果并把重试变成第二次更新。
CREATE TABLE online_profile_idempotency (
    player_id TEXT NOT NULL,
    operation TEXT NOT NULL CHECK (operation = 'UpdateCurrentPlayerProfile'),
    idempotency_key TEXT NOT NULL CHECK (char_length(idempotency_key) BETWEEN 1 AND 128),
    canonical_request BYTEA NOT NULL CHECK (octet_length(canonical_request) > 0),
    response_snapshot JSONB NOT NULL CHECK (jsonb_typeof(response_snapshot) = 'object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (player_id, operation, idempotency_key)
);

COMMENT ON TABLE online_profile_idempotency IS '玩家资料更新的持久结果；按认证主体、操作、幂等键隔离，禁止脱离资料事务单独写入';
COMMENT ON COLUMN online_profile_idempotency.canonical_request IS 'UTF-8 JSON规范请求：trim后的displayName与原expectedRevision；按字节比较，不接受同键异内容';
COMMENT ON COLUMN online_profile_idempotency.response_snapshot IS '第一次成功提交时的完整资料快照；重放不读取之后修改的资料覆盖原结果';
