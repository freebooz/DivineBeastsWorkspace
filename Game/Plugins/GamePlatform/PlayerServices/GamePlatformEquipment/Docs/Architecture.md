# Architecture（架构）

Equipment（装备）采用三权威边界：Inventory（背包）拥有 ItemInstance；PlayerData Equipment 保存长期槽位；UE EquipmentServer 管当前局内合法性和 Gameplay（玩法）应用；EquipmentClient 只做视觉表现。

三个模块分别是 Runtime（共享）、ClientOnly（仅客户端）和 ServerOnly（仅服务器）。共享模块不能依赖 HTTP、数据库或客户端资产；Server 不依赖 Client；Client 不包含后端写入或 GAS 授予逻辑。

DBAServer（神兽联盟服务端组合层）实现 `IGamePlatformEquipmentPersistencePort（装备持久化端口）`的 HTTP 适配。