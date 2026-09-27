# ValidationAndReview（验证与审查）

项目Arena验证分为三层：

1. Source/Architecture Static（源码/架构静态）：模块命名唯一、依赖方向、五模式结构、MainArena单Role/Experience、Client/Server模块隔离、无旧Element/FiveCamp/Pantheon规则。
2. Production Configuration Gate（生产配置门禁）：MapId、Selection/Spawn/Respawn/Score/Win、TimeLimit、ProjectRuleRevision、ContentRevision、HeroCatalogRevision、PickBan、DuplicateHero、Overtime、SuddenDeath必须全部明确。
3. Runtime Evidence（运行证据）：UE5.8 Build、Automation、Dedicated Server 1v1~5v5、Client/Server Cook、Hero资格Provider、GameplayLifecycleAdapter、PostMatch E2E和性能。

当前第1层通过；第2层明确失败/阻塞；第3层大部分未执行。

DeveloperTools继续承担通用依赖/Definition/StableId/ServerAssetSafety检查；项目专项脚本承担“恰好五模式、模式人数、Production不猜值、单Server Target、CharacterId身份链”等项目约束。
