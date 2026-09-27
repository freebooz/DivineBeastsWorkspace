# ModuleCreationGates（模块创建门禁）

## Client Gate：通过

| Requirement（需求） | ExistingGamePlatformArenaClientCapability（平台已有能力） | ProjectSpecificDifference（项目差异） | Consumer（消费者） | CanUseDataOnly?（仅数据可表达） | NeedsNativeClientModule?（需要原生Client模块） | Decision（结论） |
| --- | --- | --- | --- | --- | --- | --- |
| 五模式匹配入口 | 平台提供通用FGamePlatformArenaMatchmakingRequest | 必须先验证神兽联盟Production配置门禁，禁止FoundationTest/未批准Rule进入正式匹配 | 项目客户端匹配入口 | 否；需要在请求构建前执行项目Release Gate | 是 | 创建DivineBeastsArenaClient |
| ApplicationFlow竞技扩展 | 平台ArenaClient不认识DivineBeastsApplicationFlow | 需要注册IDivineBeastsApplicationFlowExtension并维护进入/离开世界状态 | DivineBeastsApplicationFlow客户端组合 | 否；需要原生Modular/Subsystem生命周期 | 是 | 创建DivineBeastsArenaClient |
| PostMatch返回世界 | 平台ArenaClient不负责项目OpenWorld回流 | 必须统一调用RequestPostMatchReturnToWorld重新获取Assignment/Ticket，禁止复用旧endpoint/ticket | 项目PostMatch流程 | 否 | 是 | 创建DivineBeastsArenaClient |
| UI展示 | 平台ViewModel可表达通用阶段/比分 | 无项目Widget需求 | 未来UI | 是 | 否 | 不放入Client模块 |

因此 `DivineBeastsArenaClient（项目竞技客户端适配模块）` 有真实原生职责，不是空模块。它不实现UI、Matchmaking算法或服务器规则。

## Server Gate：通过

| Rule（规则） | ExistingGamePlatformArenaServerCapability（平台已有能力） | CanUsePolicyData?（可仅用策略数据） | CanUsePlatformWinCondition?（可用平台胜负策略） | CanUsePlatformScorePolicy?（可用平台评分策略） | CanUseProjectDefinitionOnly?（仅项目Definition足够） | NeedsNativeServerCode?（需要原生Server代码） | Consumer（消费者） | Decision（结论） |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Production Assignment Gate | 平台能校验通用Mode/Team/Map/Roster | 部分 | 是 | 是 | 否；还需在应用Assignment前比较ProjectRule/Content/HeroCatalog Revision | 是 | MainArena Server Coordinator | 创建DivineBeastsArenaServer |
| FoundationTest隔离 | 平台内置测试Mode可用于Foundation验证 | 否 | 不适用 | 不适用 | 否；必须在项目Server边界拒绝测试配置冒充Production | 是 | MainArena Dedicated Server | 创建DivineBeastsArenaServer |
| Hero资格 | 平台有IGamePlatformArenaHeroEligibilityProvider接口 | 否；资格依赖可信Entitlement/Maintenance Provider | 不适用 | 不适用 | 否 | 是 | HeroSelection服务器权威路径 | 创建DivineBeastsArenaServer |
| Spawn/Respawn规则 | 平台有GameplayLifecycleAdapter接口 | 是 | 不适用 | 不适用 | 当前项目规则仍NotConfigured | 仅需验证Adapter存在，不重写Spawn | MainArena服务器 | 复用平台生命周期；Server模块Fail Closed |
| WinCondition/Score | 平台已有WinCondition/ScorePolicy扩展点 | 是 | 是 | 是 | 当前产品值未批准 | 否（当前） | MainArena服务器 | 不创建项目Win/Score算法 |
| MatchResult项目扩展 | 平台通用Result已含当前所需Player/Character/Team/统计字段 | 是 | 不适用 | 不适用 | 是 | 否 | GameServerControl/MatchResult | 不适用 |

因此 `DivineBeastsArenaServer（项目竞技服务器扩展模块）` 的必要性来自项目Production准入门禁、可信Hero资格和生命周期Fail Closed，而不是自建WinCondition/Score/Spawn框架。它不重写通用Arena生命周期、不发奖励、不管理MMR/DB/Agones。

## MatchResult Extension Gate：不适用

现有通用MatchResult字段足够；当前无需项目Result DTO。CharacterId与HeroDefinitionId必须继续保持不同身份，结果提交复用平台既有GameServerControl/MatchResult链路。
