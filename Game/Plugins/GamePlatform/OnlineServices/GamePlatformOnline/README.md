# GamePlatformOnline（游戏平台在线插件）

`GamePlatformOnline` 是 GamePlatform（游戏平台基础层）的通用在线认证与受保护请求插件，不包含《神兽联盟》项目语义。项目层只负责提供 `ServiceOrigin（服务源地址）`、`GameId（游戏标识）`、`ClientVersion（客户端版本）` 和具体业务相对路径；密码、AccessToken（访问令牌）、RefreshToken（刷新令牌）、HTTP 安全策略、401 刷新、重试和请求预算均由平台插件管理。

## 当前实现边界

- `GamePlatformOnline` 提供稳定公共契约、配置校验、按 `UGameInstance（游戏实例）` 隔离的服务注册表和脱敏结果类型。
- `GamePlatformOnlineClient` 实现 `IGamePlatformOnlineService（在线服务门面）`，在子系统初始化/销毁时注册和反注册同一实例服务；主工程与其他平台插件不再维护第二套认证 Provider。
- 客户端私有 `FGamePlatformGatewayAuthProvider（平台网关认证提供者）` 持有原始 Token，并统一执行 HTTPS/开发回环地址限制、拒绝自动重定向、请求/响应字节预算、流式响应上限、JSON 解析、Authorization 注入和 Idempotency-Key（幂等键）注入。
- 认证与受保护请求支持：并发/排队上限、总截止时间、单次尝试超时、主动临期刷新、401 single-flight（单次共享刷新）、安全读取有限重试、指数退避与抖动、403 不刷新、幂等写最多重放一次、非幂等写结果不确定保护、Owner/World 生命周期取消及脱敏诊断。
- `GamePlatformOnlineServer（在线服务器模块）` 已删除。Dedicated Server（专用服务器）注册/心跳/Ready/Drain 由 `GamePlatformServer（平台服务器插件）` 负责；游戏服务器连接/跨服/重连由 `GamePlatformSession（平台会话插件）` 负责。
- `Private/Requests/OnlineSession` 已通过 `GAMEPLATFORM_ONLINE_NATIVE_TEST` 编译期门禁降级为历史原生回归模型，不进入 UE 正式运行时；当前正式运行时只有 `UGamePlatformOnlineClientSubsystem + 私有 Gateway Transport` 一套生产状态机。

## 验证边界

原生 `OnlineLogicTests` 已重新配置、编译并执行通过，可证明纯逻辑回归。UE 自动化测试已覆盖真实 `GameInstance → OnlineClientSubsystem → 公共门面` 生产路径及登录、刷新、401 重放、读取重试、幂等校验、非幂等写和退出等关键行为；是否通过必须以当前 UE5.8 实际执行结果为准。Editor/Client/Server 三目标构建、真实 HTTPS Gateway 联调、网络异常和发布环境验证仍是正式交付门槛。

历史迁移与旧安全断点仅作为证据保留，不能覆盖当前源码事实。
