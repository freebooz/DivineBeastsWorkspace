-- 000012：已有真实准入绑定的服务器租约续期，不负责签发/创建玩家身份或首次进服授权。
-- 依赖000003（真实预留/领取/提交）、000005（在线账号会话）、000011（可信背包事件）。
-- 只允许已提交Admitted且当前未过期的同玩家、同服务器、同Boot、同连接、同SessionEpoch续租。
-- 不能凭借PlayerId/GameServerId字符串创建新的绑定；在线登出、封禁及实例失去Ready立即失败。
CREATE FUNCTION session_renew_inventory_player(
    p_game_id TEXT, p_player_id TEXT, p_server_id TEXT, p_boot_id TEXT,
    p_epoch BIGINT, p_connection_id TEXT, p_lease_seconds INTEGER
)
RETURNS TIMESTAMPTZ LANGUAGE plpgsql AS $$
DECLARE
    v_now TIMESTAMPTZ := clock_timestamp();
    v_instance_healthy_until TIMESTAMPTZ;
    v_login_valid_until TIMESTAMPTZ;
    v_new_expiry TIMESTAMPTZ;
BEGIN
    -- 与session_reserve/session_commit/session_release使用同一个既有全局串行化门禁，
    -- 不引入第二套锁体系，避免迁服与续租在不同事务中交错获取所有权。
    PERFORM pg_advisory_xact_lock(73031001);
    IF p_game_id IS NULL OR p_game_id = '' OR p_player_id IS NULL OR p_player_id = ''
       OR p_server_id IS NULL OR p_server_id = '' OR p_boot_id IS NULL OR p_boot_id = ''
       OR p_connection_id IS NULL OR p_connection_id = '' OR p_epoch IS NULL OR p_epoch < 1
       OR p_lease_seconds IS NULL OR p_lease_seconds < 1 OR p_lease_seconds > 30 THEN
        RAISE EXCEPTION 'SESSION_RENEW_INVALID';
    END IF;

    SELECT i.healthy_until, os.refresh_expires_at
      INTO v_instance_healthy_until, v_login_valid_until
      FROM session_bindings b
      JOIN session_instances i
        ON i.instance_id = b.instance_id AND i.boot_id = b.boot_id AND i.game_id = b.game_id
      JOIN session_reservations r
        ON r.reservation_id = b.reservation_id AND r.instance_id = b.instance_id
        AND r.boot_id = b.boot_id AND r.state = 'Admitted'
        AND r.admitted_epoch = b.session_epoch AND r.connection_id = b.connection_id
      JOIN session_authorizations a
        ON a.authorization_id = r.authorization_id AND a.game_id = b.game_id
        AND a.player_id = b.player_id AND NOT a.revoked
      JOIN online_identity_sessions os
        ON os.session_id = a.auth_session_id AND os.game_id = b.game_id
        AND os.player_id = b.player_id AND NOT os.revoked
      JOIN online_identity_accounts account
        ON account.game_id = b.game_id AND account.player_id = b.player_id
        AND NOT account.disabled
      WHERE b.game_id = p_game_id AND b.player_id = p_player_id
        AND b.instance_id = p_server_id AND b.boot_id = p_boot_id
        AND b.session_epoch = p_epoch AND b.connection_id = p_connection_id
        AND b.authority_until > v_now
        AND i.ready AND i.healthy_until > v_now
        AND os.refresh_expires_at > v_now
      FOR UPDATE OF b;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'SESSION_RENEW_DENIED';
    END IF;

    v_new_expiry := LEAST(v_now + make_interval(secs => p_lease_seconds),
                         v_instance_healthy_until, v_login_valid_until);
    IF v_new_expiry <= clock_timestamp() THEN
        RAISE EXCEPTION 'SESSION_RENEW_EXPIRED';
    END IF;

    UPDATE session_bindings SET authority_until = v_new_expiry
      WHERE game_id = p_game_id AND player_id = p_player_id
        AND instance_id = p_server_id AND boot_id = p_boot_id
        AND session_epoch = p_epoch AND connection_id = p_connection_id
        AND authority_until > clock_timestamp();
    IF NOT FOUND THEN
        RAISE EXCEPTION 'SESSION_RENEW_STALE';
    END IF;
    RETURN v_new_expiry;
END $$;

-- 只有持有受审迁移所有者/明确GRANT的后台角色可以续租；
-- 禁止给PUBLIC角色授予Execute，客户端也没有任意SQL或此函数入口。
REVOKE ALL ON FUNCTION session_renew_inventory_player(
    TEXT,TEXT,TEXT,TEXT,BIGINT,TEXT,INTEGER) FROM PUBLIC;
