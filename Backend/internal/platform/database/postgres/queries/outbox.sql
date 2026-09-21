-- Outbox SQL（事务发件箱SQL）是生产适配器与未来sqlc生成的统一语义来源。

-- name: InsertOutboxMessage :exec
INSERT INTO outbox_messages (message_id, topic, aggregate_id, payload, occurred_at)
VALUES ($1, $2, $3, $4, $5)
ON CONFLICT (message_id) DO NOTHING;

-- name: ClaimPendingOutboxMessages :many
WITH candidates AS (
    SELECT message_id
    FROM outbox_messages
    WHERE state = 'pending'
       OR (state = 'publishing' AND locked_until < NOW())
    ORDER BY occurred_at, message_id
    LIMIT $1
    FOR UPDATE SKIP LOCKED
)
UPDATE outbox_messages AS o
SET state = 'publishing', lock_owner = $2, locked_until = $3
FROM candidates c
WHERE o.message_id = c.message_id
RETURNING o.message_id, o.topic, o.aggregate_id, o.payload, o.occurred_at,
          o.state, o.attempts, o.published_at, o.lock_owner, o.locked_until;

-- name: MarkOutboxMessagePublished :exec
UPDATE outbox_messages
SET state = 'published', published_at = $3, lock_owner = '', locked_until = NULL
WHERE message_id = $1 AND state = 'publishing' AND lock_owner = $2;

-- name: MarkOutboxMessageFailed :exec
UPDATE outbox_messages
SET attempts = attempts + 1, state = 'pending', lock_owner = '', locked_until = NULL
WHERE message_id = $1 AND state = 'publishing' AND lock_owner = $2;
