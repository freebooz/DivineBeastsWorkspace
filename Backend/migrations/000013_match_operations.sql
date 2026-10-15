-- 000013：比赛跨服务操作日志，需先应用000002发件箱；仅可信MatchService读取票据秘密。
-- 活动记录不按TTL删除；完成记录后续按已批准审计保留策略归档，不能靠缓存到期释放玩家或实例。
CREATE TABLE match_operations (
    operation_id TEXT PRIMARY KEY CHECK (char_length(operation_id) BETWEEN 1 AND 128),
    input_fingerprint TEXT NOT NULL CHECK (char_length(input_fingerprint)=64),
    version BIGINT NOT NULL CHECK (version>0),
    fence BIGINT NOT NULL CHECK (fence>0),
    lease_owner TEXT NOT NULL DEFAULT '',
    lease_until TIMESTAMPTZ,
    stage TEXT NOT NULL CHECK (stage IN ('allocating','signing','committed','inMatch','returning','rollingBack','cancelled','completed')),
    payload JSONB NOT NULL CHECK (jsonb_typeof(payload)='object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_match_operations_recovery ON match_operations(stage,lease_until)
 WHERE stage NOT IN ('cancelled','completed');
COMMENT ON TABLE match_operations IS '后端比赛分配/签票/安全返回持久步骤，版本和栅栏租约拒绝迟到工作者；payload包含短期票据秘密，禁止进入日志或公共事件';
COMMENT ON COLUMN match_operations.payload IS '完整恢复状态，仅MatchService后端访问；Outbox只发布operationId/stage/version/fence';
REVOKE ALL ON match_operations FROM PUBLIC;
