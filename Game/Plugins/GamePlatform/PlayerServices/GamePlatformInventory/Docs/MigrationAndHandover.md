# MigrationAndHandover（迁移与交接）

实施前 `GamePlatformInventory`已稳定在 `GameFoundation/PlayerServices`，只有 `GamePlatformInventoryClient（背包客户端模块）`骨架；本轮保持该路径，没有按旧提示词移动到 Gameplay，也没有新增空共享/服务端 UE 模块。

后端复用 `Backend/gameplatform/inventory`和现有 PlayerDataService；没有新建 `Backend/internal/modules/inventory`第二套领域，也没有新增第六个服务。

数据库新增 `0002_player_inventory`，没有修改 Quest `0001`。Shared Contract 新增 Inventory Gateway/PlayerData API、Grant/Consume Outbox Schema（事务外发结构）、DivineBeasts Item Catalog（神兽联盟物品目录）和 Quest Reward Map（任务奖励映射）。

下一插件 `GamePlatformEntitlement（游戏平台权益插件）`仍独立；Equipment（装备）、Economy（经济钱包）也不由 Inventory 越界实现。
