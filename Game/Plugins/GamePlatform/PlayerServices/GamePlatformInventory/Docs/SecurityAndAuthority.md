# SecurityAndAuthority（安全与权威）

UE `GamePlatformInventoryClient（背包客户端模块）`只访问 Gateway `/v1/inventory...`，持有外部 AccessToken（访问令牌），不传任意 PlayerId，不包含 PlayerData 内部 Token、X-Game-Server-Id、X-Bound-Player-Id 或数据库连接。

Gateway 通过 `PlayerAuthenticator（玩家认证器）`解析当前登录玩家，并向 PlayerData 传 `X-Gateway-Caller（网关调用方） + X-Authenticated-Player-Id（已认证玩家）`。PlayerData 用 URL gameId/playerId 覆盖 Body 身份。

Grant/Consume 仅可信 Dedicated Server（专用服务器）内部路由可调用，要求 Internal Bearer Token + X-Game-Server-Id + X-Bound-Player-Id。当前仍没有 GameServerControl/Session（游戏服务器控制/会话）提供的真实“ServerInstance拥有Player”校验器，因此完整绑定认证未执行。

客户端没有 Grant API，日志源码不输出 Token；货币/Economy（经济钱包）不作为 Inventory Item。
