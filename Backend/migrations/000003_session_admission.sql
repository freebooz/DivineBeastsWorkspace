-- 会话准入持久化内核。仅在隔离库验证；身份/角色授权和真实服务器连接适配尚未接通。
-- 单一事务咨询锁提供当前最小串行化边界，不能据此声称达到生产吞吐量。
CREATE TABLE session_authorizations (
    authorization_id TEXT PRIMARY KEY, -- 经身份与玩家服务核验后产生的短期授权决策身份，不是认证令牌。
    game_id TEXT NOT NULL,
    player_id TEXT NOT NULL,
    auth_session_id TEXT NOT NULL,
    character_id TEXT NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL, -- 授权有效上限，预约和绑定租约不得越过。
    revoked BOOLEAN NOT NULL DEFAULT FALSE
);
CREATE TABLE session_instances (
    instance_id TEXT PRIMARY KEY,
    boot_id TEXT NOT NULL, -- 每次实例启动唯一；不由玩家生成。
    game_id TEXT NOT NULL,
    world_id TEXT NOT NULL,
    protocol_version TEXT NOT NULL,
    capacity INTEGER NOT NULL CHECK (capacity > 0),
    ready BOOLEAN NOT NULL DEFAULT FALSE,
    healthy_until TIMESTAMPTZ NOT NULL, -- 真实服务器心跳续租；本迁移不插入Ready实例。
    UNIQUE (instance_id, boot_id)
);
CREATE TABLE session_bindings (
    game_id TEXT NOT NULL,
    player_id TEXT NOT NULL,
    session_epoch BIGINT NOT NULL DEFAULT 0 CHECK (session_epoch >= 0),
    reservation_id TEXT NOT NULL DEFAULT '',
    instance_id TEXT NOT NULL DEFAULT '',
    boot_id TEXT NOT NULL DEFAULT '',
    connection_id TEXT NOT NULL DEFAULT '', -- 服务器握手生成且绑定UNetConnection的关联身份；不是客户端AttemptId。
    authority_until TIMESTAMPTZ NOT NULL DEFAULT '-infinity',
    PRIMARY KEY (game_id, player_id)
);
CREATE TABLE session_reservations (
    reservation_id TEXT PRIMARY KEY,
    operation_id TEXT NOT NULL UNIQUE, -- 不允许同操作身份换主体/换参数。
    authorization_id TEXT NOT NULL REFERENCES session_authorizations,
    instance_id TEXT NOT NULL,
    boot_id TEXT NOT NULL,
    attempt_id TEXT NOT NULL,
    protocol_version TEXT NOT NULL,
    ttl_seconds INTEGER NOT NULL CHECK (ttl_seconds BETWEEN 1 AND 120),
    credential_digest BYTEA NOT NULL CHECK (octet_length(credential_digest) = 32), -- 只保存SHA-256摘要。
    expected_epoch BIGINT NOT NULL,
    state TEXT NOT NULL CHECK (state IN ('Reserved','Claimed','Admitted','Released')),
    expires_at TIMESTAMPTZ NOT NULL,
    connection_id TEXT NOT NULL DEFAULT '',
    admitted_epoch BIGINT,
    FOREIGN KEY (instance_id, boot_id) REFERENCES session_instances (instance_id, boot_id)
);
CREATE INDEX session_reservation_capacity ON session_reservations(instance_id, boot_id, state, expires_at);

-- 所有调用由同一受控控制面事务访问；该函数不替代HTTP/mTLS服务身份认证。
CREATE FUNCTION session_reserve(reservation TEXT, operation TEXT, authorization_key TEXT,
    target TEXT, boot TEXT, attempt TEXT, digest BYTEA, protocol TEXT, ttl_seconds INTEGER)
RETURNS TEXT LANGUAGE plpgsql AS $$
DECLARE grant_row session_authorizations; instance_row session_instances; existing session_reservations;
    binding_row session_bindings; used_slots INTEGER; instant TIMESTAMPTZ;
BEGIN
    PERFORM pg_advisory_xact_lock(73031001);
    instant := clock_timestamp();
    IF reservation = '' OR operation = '' OR attempt = '' OR digest IS NULL OR octet_length(digest) <> 32
        OR ttl_seconds IS NULL OR ttl_seconds < 1 OR ttl_seconds > 120 THEN RAISE EXCEPTION 'SESSION_INVALID'; END IF;
    SELECT * INTO grant_row FROM session_authorizations WHERE authorization_id = authorization_key AND NOT revoked AND expires_at > instant;
    IF NOT FOUND THEN RAISE EXCEPTION 'SESSION_UNAUTHORIZED'; END IF;
    SELECT * INTO existing FROM session_reservations WHERE operation_id = operation;
    IF FOUND THEN
        IF existing.authorization_id <> authorization_key OR existing.reservation_id <> reservation OR existing.instance_id <> target
            OR existing.boot_id <> boot OR existing.attempt_id <> attempt OR existing.credential_digest <> digest
            OR existing.protocol_version <> protocol OR existing.ttl_seconds <> ttl_seconds THEN
            RAISE EXCEPTION 'SESSION_IDEMPOTENCY_CONFLICT';
        END IF;
        RETURN existing.state;
    END IF;
    SELECT * INTO instance_row FROM session_instances WHERE instance_id = target AND boot_id = boot
        AND game_id = grant_row.game_id AND protocol_version = protocol AND ready AND healthy_until > instant;
    IF NOT FOUND THEN RAISE EXCEPTION 'SESSION_TARGET_UNAVAILABLE'; END IF;
    INSERT INTO session_bindings(game_id, player_id) VALUES(grant_row.game_id, grant_row.player_id) ON CONFLICT DO NOTHING;
    SELECT * INTO binding_row FROM session_bindings WHERE game_id = grant_row.game_id AND player_id = grant_row.player_id FOR UPDATE;
    IF EXISTS (SELECT 1 FROM session_reservations reservation_row JOIN session_authorizations auth_row USING(authorization_id)
        WHERE auth_row.game_id = grant_row.game_id AND auth_row.player_id = grant_row.player_id
        AND reservation_row.state IN ('Reserved','Claimed') AND reservation_row.expires_at > instant) THEN
        RAISE EXCEPTION 'SESSION_BUSY';
    END IF;
    SELECT count(*) INTO used_slots FROM session_reservations WHERE instance_id = target AND boot_id = boot
        AND state IN ('Reserved','Claimed') AND expires_at > instant;
    used_slots := used_slots + (SELECT count(*) FROM session_bindings WHERE instance_id = target AND boot_id = boot AND authority_until > instant);
    IF used_slots >= instance_row.capacity THEN RAISE EXCEPTION 'SESSION_NO_CAPACITY'; END IF;
    INSERT INTO session_reservations VALUES(reservation,operation,authorization_key,target,boot,attempt,protocol,ttl_seconds,digest,binding_row.session_epoch,
        'Reserved',LEAST(instant + make_interval(secs => ttl_seconds), grant_row.expires_at),'',NULL);
    RETURN 'Reserved';
END $$;

-- 领取同一材料时，只有同一实例启动代次、同一服务器真实连接关联可以幂等重试。
CREATE FUNCTION session_claim(reservation TEXT, target TEXT, boot TEXT, connection TEXT, attempt TEXT, digest BYTEA)
RETURNS TEXT LANGUAGE plpgsql AS $$
DECLARE item session_reservations; instant TIMESTAMPTZ;
BEGIN
    PERFORM pg_advisory_xact_lock(73031001);
    instant := clock_timestamp();
    SELECT * INTO item FROM session_reservations WHERE reservation_id = reservation FOR UPDATE;
    IF NOT FOUND OR target IS NULL OR boot IS NULL OR attempt IS NULL OR connection IS NULL OR connection = '' OR digest IS NULL OR item.credential_digest <> digest
        OR item.instance_id <> target OR item.boot_id <> boot OR item.attempt_id <> attempt THEN RAISE EXCEPTION 'SESSION_CLAIM_REJECTED'; END IF;
    IF NOT EXISTS(SELECT 1 FROM session_instances WHERE instance_id = target AND boot_id = boot AND ready AND healthy_until > instant)
        OR NOT EXISTS(SELECT 1 FROM session_authorizations WHERE authorization_id = item.authorization_id AND NOT revoked AND expires_at > instant) THEN
        RAISE EXCEPTION 'SESSION_AUTHORITY_EXPIRED';
    END IF;
    IF item.state = 'Claimed' AND item.connection_id = connection AND item.expires_at > instant THEN RETURN 'Claimed'; END IF;
    IF item.state <> 'Reserved' OR item.expires_at <= instant THEN RAISE EXCEPTION 'SESSION_REPLAY_OR_EXPIRED'; END IF;
    UPDATE session_reservations SET state = 'Claimed', connection_id = connection WHERE reservation_id = reservation;
    RETURN 'Claimed';
END $$;

-- 连接提交只在来源权威租约已结束后成功，原子转换预留为带栅栏的绑定。
CREATE FUNCTION session_commit(reservation TEXT, target TEXT, boot TEXT, connection TEXT, lease_seconds INTEGER)
RETURNS BIGINT LANGUAGE plpgsql AS $$
DECLARE item session_reservations; grant_row session_authorizations; binding_row session_bindings; instant TIMESTAMPTZ;
BEGIN
    PERFORM pg_advisory_xact_lock(73031001);
    instant := clock_timestamp();
    SELECT * INTO item FROM session_reservations WHERE reservation_id = reservation FOR UPDATE;
    IF NOT FOUND OR target IS NULL OR boot IS NULL OR connection IS NULL OR item.instance_id <> target OR item.boot_id <> boot OR item.connection_id <> connection OR connection = ''
        OR lease_seconds IS NULL OR lease_seconds < 1 OR lease_seconds > 30 THEN RAISE EXCEPTION 'SESSION_COMMIT_REJECTED'; END IF;
    SELECT * INTO grant_row FROM session_authorizations WHERE authorization_id = item.authorization_id AND NOT revoked AND expires_at > instant;
    IF NOT FOUND THEN RAISE EXCEPTION 'SESSION_UNAUTHORIZED'; END IF;
    IF NOT EXISTS(SELECT 1 FROM session_instances WHERE instance_id = target AND boot_id = boot AND ready AND healthy_until > instant) THEN
        RAISE EXCEPTION 'SESSION_TARGET_UNAVAILABLE';
    END IF;
    SELECT * INTO binding_row FROM session_bindings WHERE game_id = grant_row.game_id AND player_id = grant_row.player_id FOR UPDATE;
    IF item.state = 'Admitted' AND binding_row.reservation_id = reservation AND binding_row.session_epoch = item.admitted_epoch
        AND binding_row.authority_until > instant THEN RETURN item.admitted_epoch; END IF;
    IF item.state <> 'Claimed' OR item.expires_at <= instant OR binding_row.session_epoch <> item.expected_epoch THEN
        RAISE EXCEPTION 'SESSION_STALE_OR_EXPIRED';
    END IF;
    IF binding_row.authority_until > instant THEN RAISE EXCEPTION 'SESSION_SOURCE_STILL_AUTHORITATIVE'; END IF;
    UPDATE session_bindings SET session_epoch = session_epoch + 1, reservation_id = reservation, instance_id = target, boot_id = boot,
        connection_id = connection, authority_until = LEAST(instant + make_interval(secs => lease_seconds), grant_row.expires_at)
        WHERE game_id = grant_row.game_id AND player_id = grant_row.player_id RETURNING * INTO binding_row;
    UPDATE session_reservations SET state = 'Admitted', admitted_epoch = binding_row.session_epoch WHERE reservation_id = reservation;
    RETURN binding_row.session_epoch;
END $$;

-- 服务器离开只影响自己确切的绑定代次；旧来源报告不得清除新目标。
CREATE FUNCTION session_release(reservation TEXT, target TEXT, boot TEXT, connection TEXT, epoch BIGINT)
RETURNS BOOLEAN LANGUAGE plpgsql AS $$
DECLARE item session_reservations; affected INTEGER;
BEGIN
    PERFORM pg_advisory_xact_lock(73031001);
    SELECT * INTO item FROM session_reservations WHERE reservation_id = reservation FOR UPDATE;
    IF NOT FOUND OR target IS NULL OR boot IS NULL OR connection IS NULL OR epoch IS NULL
        OR item.instance_id <> target OR item.boot_id <> boot OR item.connection_id <> connection THEN RETURN FALSE; END IF;
    IF item.state = 'Released' THEN RETURN item.admitted_epoch IS NOT DISTINCT FROM epoch; END IF;
    IF item.state <> 'Admitted' OR item.admitted_epoch <> epoch THEN RETURN FALSE; END IF;
    UPDATE session_bindings SET authority_until = '-infinity' WHERE reservation_id = reservation AND session_epoch = epoch
        AND instance_id = target AND boot_id = boot AND connection_id = connection;
    GET DIAGNOSTICS affected = ROW_COUNT;
    IF affected = 1 THEN UPDATE session_reservations SET state = 'Released' WHERE reservation_id = reservation; END IF;
    RETURN affected = 1;
END $$;

-- 玩家撤销必须使用当前核验所得的授权决策，提交先赢则不能以取消回滚正式绑定。
CREATE FUNCTION session_cancel(reservation TEXT, authorization_key TEXT) RETURNS TEXT LANGUAGE plpgsql AS $$
DECLARE item session_reservations;
BEGIN
    PERFORM pg_advisory_xact_lock(73031001);
    SELECT * INTO item FROM session_reservations WHERE reservation_id = reservation FOR UPDATE;
    IF NOT FOUND OR authorization_key IS NULL OR item.authorization_id <> authorization_key THEN RAISE EXCEPTION 'SESSION_UNAUTHORIZED'; END IF;
    IF item.state = 'Admitted' THEN RETURN 'Admitted'; END IF;
    UPDATE session_reservations SET state = 'Released' WHERE reservation_id = reservation;
    RETURN 'Released';
END $$;

-- 迁移不给PUBLIC调用权。正式数据库角色GRANT由未来受审查部署执行；默认只允许迁移所有者。
REVOKE ALL ON session_authorizations, session_instances, session_bindings, session_reservations FROM PUBLIC;
REVOKE ALL ON FUNCTION session_reserve(TEXT,TEXT,TEXT,TEXT,TEXT,TEXT,BYTEA,TEXT,INTEGER) FROM PUBLIC;
REVOKE ALL ON FUNCTION session_claim(TEXT,TEXT,TEXT,TEXT,TEXT,BYTEA) FROM PUBLIC;
REVOKE ALL ON FUNCTION session_commit(TEXT,TEXT,TEXT,TEXT,INTEGER) FROM PUBLIC;
REVOKE ALL ON FUNCTION session_release(TEXT,TEXT,TEXT,TEXT,BIGINT) FROM PUBLIC;
REVOKE ALL ON FUNCTION session_cancel(TEXT,TEXT) FROM PUBLIC;
