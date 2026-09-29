# Architecture（架构）

## 分层定位

`GamePlatformOnline（游戏平台在线插件）` 属于第一层 GamePlatform，只提供跨游戏可复用的在线认证、Token 生命周期、同源受保护请求和诊断能力。它不得依赖 MobaCommon（MOBA 通用层）或 DivineBeasts（神兽联盟项目层），也不承担 Dedicated Server 控制面或游戏会话连接职责。

依赖方向固定为：`DivineBeasts → MobaCommon → GamePlatform`；项目层允许直接使用平台层 Online。

## 模块职责

- `GamePlatformOnline`：稳定公共契约、配置、结果、认证/资料类型、实例服务注册表和纯逻辑回归基线，不依赖 HTTP/JSON。
- `GamePlatformOnlineClient`：Client/Editor 专用 UE 适配，`UGamePlatformOnlineClientSubsystem` 实现 `IGamePlatformOnlineService`，负责 GameInstance 生命周期、游戏线程提交、请求调度、认证刷新和事件投影。
- `Private/Transport/GamePlatformGatewayAuthProvider`：私有 HTTP/JSON 传输实现，唯一持有 AccessToken/RefreshToken；项目层和 UI 永远不能读取 Token 原文。

## 生命周期与安全

每个 GameInstance 独立维护认证上下文。登录、退出和失效化推进 `AuthGeneration（认证代次）`；迟到回调只有代次仍匹配时才能提交。Provider 回调允许来自任意线程，但 UObject 状态只在游戏线程修改。

受保护请求只接受同源相对路径；客户端传输拒绝自动重定向，响应正文在接收过程中执行字节上限，未知或不支持所需安全能力时失败关闭。401 只允许一次共享刷新；403 不触发刷新。GET/HEAD 可按受限读取策略重试；写请求仅在业务声明幂等且提供幂等键时允许刷新后重放一次。非幂等写若可能已到达服务端，则必须返回 `OutcomeUnknown（结果不确定）`，禁止盲目重放。

## 与其他插件边界

- `GamePlatformSession`：负责服务器分配、ClientTravel（客户端旅行）、Admission（准入）、跨服和游戏连接重连，不持有 Online Token。
- `GamePlatformServer`：负责 Dedicated Server 注册、Heartbeat（心跳）、Ready（就绪）和 Drain（排空）。
- `GamePlatformTelemetry` 等其他平台插件如需认证，只通过窄的请求授权能力附加认证，不读取 Token 字符串。

## 状态机收敛

历史 `Private/Requests/OnlineSession` 仅在 `GAMEPLATFORM_ONLINE_NATIVE_TEST` 下编译，用于保留迁移前纯逻辑回归基线，禁止 UE 正式运行时重新引用。当前生产路径只有 `UGamePlatformOnlineClientSubsystem + FGamePlatformGatewayAuthProvider` 一套状态机。后续若进一步抽取纯逻辑 Kernel，应从当前生产路径提炼并让子系统直接复用，而不是恢复历史双实现。