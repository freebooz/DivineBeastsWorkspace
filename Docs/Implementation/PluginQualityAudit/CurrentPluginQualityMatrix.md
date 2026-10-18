# SSLM 全量插件质量与可视化人工验收审查台账

> 来源：实际工作树67个插件描述、源码、资源及正式双台账；静态盘点不替代UE编译、蓝图验收、Unreal Insights性能实测或人工签收。

## 基线与处理顺序

- 代码插件48、内容包19、声明模块89；已登记演示功能4、场景9，人工签收0。
- 要求可视化/交互人工验证的插件53，尚无登记操作案例的51。功能总分母未逐项评审，不能计算真实覆盖率。
- 有本模块原生测试源码的插件45。没有本地文件不等于项目不存在跨模块自动化测试。
- 已关联真实诊断图的插件19、不重复的诊断图13张；只是进入地图的观察入口，不计入正式演示案例和人工签收。

| 插件 | 层级/类别 | 模块 | 源码(.cpp) | 原生资产/地图 | 测试源 | 要求人工界面 | 诊断图快捷入口 | 正式演示案例 | 编目审查 | 优先级 |
|---|---|---:|---:|---:|---:|---|---|---:|---|---|
| DBAArena | Project/code | 3 | 24 | 6/0 | 8 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| DBAClient | Project/code | 6 | 202 | 1/0 | 62 | 是 | 有（待人工检查） | 2 | partial | P2 |
| DBAContentPack_Common | Project/content_pack | 0 | 0 | 129/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAFrontEndPack | Project/content_pack | 0 | 0 | 53/2 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAGameplay | Project/code | 3 | 42 | 229/0 | 12 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| DBAHeroPack_Boar | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Dog | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Dragon | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Goat | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Horse | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Monkey | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Ox | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Rabbit | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Rat | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Rooster | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Snake | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAHeroPack_Tiger | Project/content_pack | 0 | 0 | 9/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAPresentationPack_Core | Project/content_pack | 0 | 0 | 1780/1 | 0 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| DBAServer | Project/code | 1 | 12 | 0/0 | 5 | 否 | 无 | 0 | unreviewed | P2 |
| DBASFXPack_Core | Project/content_pack | 0 | 0 | 105/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAUIPack_Core | Project/content_pack | 0 | 0 | 194/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAWorldPack_MainArena | Project/content_pack | 0 | 0 | 4/1 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAWorldPack_Village | Project/content_pack | 0 | 0 | 204/1 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| DBAWorlds | Project/code | 1 | 7 | 0/0 | 2 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformAbilitySystem | Platform/code | 1 | 11 | 0/0 | 5 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformAI | Platform/code | 2 | 20 | 0/0 | 8 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformAnimation | Platform/code | 2 | 7 | 0/0 | 2 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformApplicationFlow | Platform/code | 1 | 10 | 0/0 | 5 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformArena | Moba/code | 5 | 29 | 0/0 | 7 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformCamera | Platform/code | 1 | 6 | 0/0 | 2 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformCharacter | Platform/code | 1 | 6 | 0/0 | 2 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformCombat | Platform/code | 1 | 18 | 0/0 | 6 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformCommerceUI | Platform/code | 1 | 8 | 0/0 | 4 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformCore | Platform/code | 1 | 8 | 0/0 | 2 | 否 | 无 | 3 | partial | P2 |
| GamePlatformData | Platform/code | 2 | 19 | 0/0 | 10 | 否 | 无 | 2 | partial | P2 |
| GamePlatformDebug | Platform/code | 2 | 7 | 0/0 | 1 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformDeveloperTools | Platform/code | 1 | 15 | 0/0 | 3 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformEntitlement | Platform/code | 2 | 7 | 0/0 | 2 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformEquipment | Platform/code | 3 | 10 | 0/0 | 3 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformGameplay | Platform/code | 1 | 14 | 0/0 | 5 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformInput | Platform/code | 1 | 11 | 0/0 | 4 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformInteraction | Platform/code | 1 | 10 | 0/0 | 5 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformInventory | Platform/code | 1 | 4 | 0/0 | 1 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformLiveOps | Platform/code | 1 | 9 | 0/0 | 5 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformLoading | Platform/code | 1 | 4 | 0/0 | 2 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformLobby | Platform/code | 3 | 3 | 0/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformLocalization | Platform/code | 1 | 1 | 0/0 | 0 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformNavigation | Platform/code | 2 | 13 | 0/0 | 4 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformOnline | Platform/code | 2 | 12 | 0/0 | 4 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformPCG | Platform/code | 2 | 32 | 0/0 | 5 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformPresentation | Platform/code | 2 | 8 | 0/0 | 2 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformProgression | Platform/code | 2 | 10 | 0/0 | 5 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformQuest | Platform/code | 3 | 14 | 0/0 | 5 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformSave | Platform/code | 1 | 7 | 0/0 | 2 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformServer | Platform/code | 1 | 15 | 0/0 | 8 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformSession | Platform/code | 1 | 7 | 0/0 | 3 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformSettings | Platform/code | 4 | 23 | 0/0 | 4 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformSFX | Platform/code | 1 | 11 | 0/0 | 4 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformSurface | Platform/code | 2 | 23 | 149/0 | 4 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformTelemetry | Platform/code | 1 | 14 | 0/0 | 4 | 否 | 无 | 0 | unreviewed | P2 |
| GamePlatformUI | Platform/code | 1 | 56 | 14/0 | 13 | 是 | 无 | 2 | partial | P2 |
| GamePlatformVFX | Platform/code | 2 | 64 | 52/0 | 21 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformVillage | Platform/code | 3 | 3 | 0/0 | 0 | 是 | 无 | 0 | unreviewed | P1 |
| GamePlatformWater | Platform/code | 3 | 9 | 0/0 | 1 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformWeather | Platform/code | 2 | 9 | 0/0 | 1 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| GamePlatformWorld | Platform/code | 2 | 16 | 0/0 | 6 | 是 | 有（待人工检查） | 0 | unreviewed | P1 |
| MobaPresentation | Moba/code | 2 | 13 | 0/0 | 4 | 是 | 无 | 0 | unreviewed | P1 |

## 审查与优化规范

- P0：描述/模块/依赖存在明确错误时阻断编译；需结合已有模块闭包、继承与Client/Server Cook门禁进一步复核。
- P1：可视化或交互插件没有登记操作案例时属于人工验收交付缺口。复用现有DemoHub与开发地图、Widget，不创建另一套入口；必须真实可进入、演示、操作、停止、重置、返回和生成待审证据。
- P2：对Tick、Ticker、同步加载、全世界Actor搜索进行源码上下文与实际Insights测量，检查多世界隔离、取消/析构、限额和失败收口；文本匹配只属于候选。
- P3：逐功能验证蓝图父类、依赖、可重入、输入、无障碍、PC/移动端、低中高画质、Dedicated Server资源隔离和多人实际运行。
- 不可视化的纯服务允许以演示中心可操作诊断页面验收，不强制3D地图；尚无功能分母/原生证据前所有插件均不得标记生产完成。

