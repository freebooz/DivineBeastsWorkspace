# DBAWorlds（神兽联盟项目世界插件）

正式位置：`Game/Plugins/DivineBeasts/DBAWorlds/`。插件拥有项目世界定义类型与Shared角色/体验的一致性约束，不强制依赖竞技。实际地图、区域定义实例、程序化世界结果与专属资源按ContentPacks规划归三个世界内容包，由UE资产工具交付，不在本插件复制第二份地图。

`DBAWorldsRuntime`派生平台`UGamePlatformWorldDefinition`，校验项目服务器角色、默认体验和可选竞技模式映射；它不分配服务器、不加载/Travel地图、不引入网络凭据，也不代替平台World/Data模块的运行时生命周期。

环境表面材质机制归第一层`GamePlatformSurface`：DBAWorlds不依赖其ClientOnly实现，也不在Runtime复制雪／苔藓／湿润／积水算法。具体`MI_DBA_*`材质实例、项目纹理和世界场景资产归对应`DBAWorldPack_*`；客户端世界表现适配可在项目客户端／内容装配层把天气或世界表现事实提交给Surface，Dedicated Server继续只消费服务器安全的世界／玩法Definition。

当前工程没有任何已交付`.uasset`或`.umap`。因此本插件不是世界内容交付完成的证明；构建后仍须创建真实地图和定义资产并通过编辑器验证、Cook/Stage及运行检查。当前交付项目定义约束源码，尚未通过实际UE构建，不伪造资产或可编译性结论。

2026-09-27修正UE测试中遗留的四角色正向夹具：大厅和旧大厅兼容体验都归OpenWorld，明确拒绝独立Lobby角色，并检查合法大厅附带竞技模式返回`ArenaModeWorldContextMismatch`。跨语言真源与正向夹具的一致性由`Tests/Architecture/ServerRoleProfiles.Tests.ps1`检查；这不替代尚未执行的UE自动化测试。
