# Architecture（架构）

`GamePlatformInventory（游戏平台背包插件）`位于 GamePlatform（平台层），只承载可跨游戏复用的背包客户端视图、请求与长期背包后端领域，不认识生肖、MOBA 竞技规则或项目资源。依赖方向保持 `DivineBeasts → MobaCommon → GamePlatform`；`DBAClient`可以消费本插件，本插件不得反向依赖项目层。

UE 侧只包含 `GamePlatformInventoryClient（背包客户端模块）`，宿主为 `ClientOnly（仅客户端）`。它是 `ULocalPlayerSubsystem（本地玩家子系统）`，自动订阅 `UGamePlatformOnlineClientSubsystem（平台在线客户端子系统）`认证状态，并通过 Online 的 `SendAuthenticatedRequest（受保护请求）`访问 Gateway。背包插件不保存 AccessToken、不维护第二套刷新逻辑、不使用进程全局 Singleton，也不承担数据库事务。

长期真源位于既有 PlayerDataService 内：领域为 `Backend/internal/modules/inventory`，PlayerData 装配桥为 `Backend/internal/modules/playerdata/inventory_bridge.go`，生产 PostgreSQL 仓储为 `Backend/internal/platform/database/postgres/inventory_repository.go`，SQL 真源为 `Backend/migrations/000008_player_inventory.sql`。没有新增第六个微服务。

普通客户端 HTTP 链为：`DBAClient UI → GamePlatformInventoryClient → GamePlatformOnlineClient → Gateway /v1/inventory... → PlayerData internal HTTP → Inventory Domain → PostgreSQL`。公网请求从 AccessToken 解析玩家身份，客户端 Body 不接受 playerId。公网所有 Revision 使用十进制字符串，Go 内部继续使用 int64。

生产 `productiondeps,grpcdeps` 模式已经把同一 Inventory PostgreSQL Repository 装配进 PlayerDataService，并更新 `Backend/internal/contracts/proto/player-data-service.proto` 协议源；但当前环境缺少 Protobuf 生成工具，新增 RPC 的生成绑定、grpcclient/grpcadapter 尚未生成，因此 Gateway 的 Inventory gRPC 内部链仍是明确未完成项。

可信 Dedicated Server 的 Grant/Consume、任务奖励和外部拾取尚未在本轮实现；未来必须通过 GameServerControl/Session 可验证的服务器实例—玩家绑定进入 PlayerData 内部权威接口，不能开放给普通客户端。
