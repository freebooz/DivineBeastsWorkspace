# SecurityAndAuthority（安全与权威）

客户端不能 Grant、Revoke、修改 ExpiresAt，也不能通过本地资产或 Unlocked 布尔值获得服务器权限。

Gateway 只读取当前 PlayerAuthenticator 解析出的玩家 Snapshot，并向 PlayerData 附带精确 GATEWAY_CALLER_ID 与 X-Authenticated-Player-Id。PlayerData 内部路由整体受 Internal Bearer Token 保护。

Grant/Revoke/Check 只允许绑定 Dedicated Server Header。完整 ServerAssignment 验证器尚未实现，因此该控制面验证保持未执行。源码不记录 Token。