-- 000002_outbox.sql（事务发件箱表）：业务数据与集成事件在同一PostgreSQL事务中持久化。
CREATE TABLE IF NOT EXISTS outbox_messages (
    message_id TEXT PRIMARY KEY,
    topic TEXT NOT NULL,
    aggregate_id TEXT NOT NULL,
    payload JSONB NOT NULL,
    occurred_at TIMESTAMPTZ NOT NULL,
    state TEXT NOT NULL DEFAULT 'pending' CHECK (state IN ('pending', 'publishing', 'published')),
    attempts INTEGER NOT NULL DEFAULT 0 CHECK (attempts >= 0),
    lock_owner TEXT NOT NULL DEFAULT '',
    locked_until TIMESTAMPTZ NULL,
    published_at TIMESTAMPTZ NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_outbox_messages_pending
    ON outbox_messages (occurred_at, message_id)
    WHERE state IN ('pending', 'publishing');
