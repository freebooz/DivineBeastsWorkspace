# ServerAuthorization（服务器授权）

DBAServer 通过 `FDBAEntitlementPlayerDataClient（权益PlayerData客户端）`异步调用 PlayerData `/entitlements/check`，请求携带 Internal Token、X-Game-Server-Id 与 X-Bound-Player-Id。

`FDBAEntitlementAuthorizationService（权益授权服务）`提供 BeginAuthorizeHero 与 BeginAuthorizeSkin。RequiredEntitlementId 为空表示免费内容；非空时必须获得后端 Entitled=true。

当前 `authorizeBoundServer（绑定服务器授权）`仍只验证内部令牌和 Header 一致性，尚未由 GameServerControl/Session 验证“该 ServerInstance 确实拥有该 Player”，所以完整控制面绑定验证未执行。