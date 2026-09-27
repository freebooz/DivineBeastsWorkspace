# Architecture（架构）

`GamePlatformInventory（游戏平台背包插件）`正式稳定路径为 `Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory`。UE 侧只有 `GamePlatformInventoryClient（背包客户端模块）`，宿主类型为 `ClientOnly（仅客户端）`；长期数量、Grant（授予）、Consume（消耗）和数据库事务不在 UE Client（客户端）执行。

长期真源是既有 `PlayerDataService（玩家数据服务） + PostgreSQL（关系数据库）`。公共后端领域位于 `Backend/gameplatform/inventory（平台背包领域）`，数据库适配位于 `Backend/gameplatform/internal/persistence/inventoryrepository（背包持久化仓储）`；没有新增 InventoryService（背包微服务）。

普通 Client（客户端）链路：`InventoryClient → Gateway（网关） → PlayerData → PostgreSQL`。可信 Dedicated Server（专用服务器）链路：`DBAServer（神兽联盟服务端组合层） → PlayerData internal Grant/Consume（内部授予/消耗）`。Quest（任务）和 Interaction（交互）不依赖 InventoryClient。

当前 Gateway 默认入口在 `PlayerAuthenticator（玩家认证器）`未实现时明确启动失败；只有注入正式认证器后 `RunWithAuthenticator（带认证器运行）`才装配 Inventory API，因此当前认证运行链仍为未执行。
