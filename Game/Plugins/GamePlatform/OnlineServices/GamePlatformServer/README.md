# GamePlatformServer（游戏平台服务器控制面生命周期）

本插件的 `UGamePlatformServerLifecycleSubsystem` 按 `GameInstance` 隔离注册、心跳、Ready 与 Drain 生命周期。模块和子系统初始化均不联网；服务器组合根完成角色Profile、世界和必要资源检查后，才可显式调用注册接口。

实际 HTTP 控制面传输由唯一 `IGamePlatformServerControlProvider` 提供者负责，并通过 Modular Features（模块化功能）注册；协议路径与请求字段对应 `Shared/Contracts/GamePlatform/OpenAPI/game-server-control.openapi.yaml`。四个生命周期接口要求 `GAMESERVERCONTROL_INTERNAL_TOKEN` Bearer 令牌及 `X-Game-Server-Id`；实例头必须与请求体编号一致。缺失或重复提供者、无效实例信息及控制面失败均拒绝Ready；令牌仅从受控进程环境读取，不写入Profile或日志。本地/生产后端的控制面部署仍须使用私有网络及经批准的传输加密终止点；共享令牌不能证明每个实例拥有不同凭据。

注册请求透传可选的 `GAME_SERVER_CLUSTER_ID`、`GAME_SERVER_NODE_ID` 和 `GAME_SERVER_PROTOCOL_VERSION` 环境值；不使用编排集群时前两者可留空，协议版本默认0。HTTP/JSON只作为私有实现依赖，不进入公开模块接口。

`Private/Tests/GamePlatformServerLifecycleTests.cpp` 覆盖注册信息边界。UE自动化测试源码已加入，但当前主机未检测到锁定版UE 5.8工具链，尚未执行编译或运行。
