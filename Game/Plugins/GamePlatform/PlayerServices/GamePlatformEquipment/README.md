# GamePlatformEquipment（游戏平台装备插件）

正式路径：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformEquipment`。

插件包含三个 UE（虚幻引擎）模块：`GamePlatformEquipment（装备共享模块）`为 Runtime（双端运行时），`GamePlatformEquipmentClient（装备客户端模块）`为 ClientOnly（仅客户端），`GamePlatformEquipmentServer（装备服务器模块）`为 ServerOnly（仅服务器）。

权威边界固定为：Inventory（背包）保存长期 ItemInstance（物品实例）所有权；PlayerData Equipment（玩家数据装备）保存 CharacterId（角色编号）维度的长期装备槽状态；UE EquipmentServer 负责当前局内合法性、GAS（玩法能力系统）应用和复制；UE EquipmentClient 只负责 Mesh/Socket/Material（网格/挂点/材质）表现。

当前源码已经实现三模块、Owner/Public Snapshot（拥有者/公共快照）、Revision（修订号）、RuntimeGeneration（运行代次）、Equip/Unequip（装备/卸下）、OperationId（操作编号）幂等、Inventory 所有权/Revision事务校验、PostgreSQL 0004 Migration（数据库迁移）、Outbox（事务外发）、DBAServer 异步 Persistence Adapter（持久化适配器）、GAS GrantHandle（授予句柄）框架、StaticMesh（静态网格）异步视觉装配和 Socket 校验。

当前限制：GamePlatformAbilitySystem（游戏平台能力系统）仍没有正式 AbilitySet（能力集合）目录/授予 API，因此非空 AbilitySet/Effect 定义需要后续真实 Resolver（解析器）才能运行；项目也没有可验证的 CharacterId↔PlayerId 权威角色归属表。未创建任何伪造 .uasset/.umap（二进制UE资产）。Go/PostgreSQL/UE5.8 运行、Dedicated Server+双客户端、Respawn、Late Join、Build/Cook 和人工审查均保持未执行。
