# GamePlatformInventory（游戏平台背包插件）

正式稳定路径：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory`。UE 侧仅包含 `GamePlatformInventoryClient（背包客户端模块）`，Type=`ClientOnly（仅客户端）`；长期背包权威位于 `PlayerDataService（玩家数据服务） + PostgreSQL（关系数据库）`。

已实现源码：Snapshot/Revision、ItemInstance、Stack/Unique、Container/Slot、Move/Swap、Split/Merge、Quickbar、Pending Operation、OperationId、ExpectedRevision、Reconcile、AccountGeneration、Gateway异步HTTP传输、取消在途请求与中立ViewModel。

后端已实现 Inventory 领域、0002 PostgreSQL Migration、事务仓储、Operation持久幂等、Grant/Consume、Outbox、Gateway/PlayerData接口、Quest Reward稳定Grant和Interaction External Pickup边界；没有新增Inventory微服务，没有实现Equipment/Entitlement/Economy。

当前 Runner 无 Go/psql/PostgreSQL/UE5.8完整工具链，且正式PlayerAuthenticator、ServerAssignment验证器和消息Publisher尚未实现，因此真实登录、DB运行、并发、Quest Reward消费、Pickup竞争、UE Build/Cook和人工审查均未执行。

