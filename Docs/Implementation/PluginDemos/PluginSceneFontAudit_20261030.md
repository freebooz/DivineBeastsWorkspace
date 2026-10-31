# SSLM 插件演示关卡与字体审查（2026-10-30）

源码基线：`main` / `09ca3e37ed0cc7c891b057d1c0a4b9ad1ecce999`。用户已明确字体范围包括插件编辑器界面和游戏运行时界面，目标为 10 号。

本报告完成插件身份、作者编目、磁盘引用和已读源码的静态审查；未完成逐插件功能完整性、UE 原生资产加载、实际渲染或人工签收。原工作树 17 个已修改文件及 6 个未跟踪项未被本次审查覆盖或清理。

## 盘点结果

真实描述文件和编目均为 70 项，身份差异为零；48 个自有代码插件、20 个内容包、2 个第三方编辑器插件。现有 288 项登记功能、211 个案例和 390 个审核用例。

151 个案例显式指向 Hub，57 个案例不指定地图，3 个案例指向鸟群场景；70/70 个独立插件页面在磁盘存在。开发目录有 50 张地图文件，插件下有 160 个 WBP 文件名候选。文件存在不代表可加载、字体达标或场景有效。

现有 `ValidatePluginDemoManifest.py --check-docs` 返回 0、issues 为空。该门禁检查结构、关系和派生文档，不证明可观察场景或 10 号字体已实现。

## 需要整改或原生核实的项目

1. **效果案例与可观察场景的对应尚未完成原生认证。** VFX、Surface、Weather、Water、PCG、Camera 和 SFX 等案例大多指向 Hub。项目已有 ManualHost、按功能适配器及动态舞台，不能仅凭复用 Hub 判为缺场景；但地形、水体、雪接触等必须逐项确认有真实对象、观察镜头和有效按钮路由。已有 Terrain/Water/Weather/Camera 等独立地图仅在磁盘发现，不得直接标为当前案例已接线。
2. **字号合同互相冲突。** 相机演示 `DivineBeastsCameraReviewWidget.cpp` 写入 14/18/13；技能栏 `DivineBeastsAbilityBarPanel.cpp` 写入 12；`ValidateFrontEndInputFonts.py` 要求 16；当前 `PluginDemoUIMonolithRecipes.json` 仍规定 18/20/28。最新明确要求取代这些当前值；历史审核记录保留原值。
3. **主题刷新可能保留旧字号。** `GamePlatformUIThemeBinding.cpp` 在 `bPreserveFontSize` 为真时恢复旧字号；项目 `DivineBeastsUIStyleBindings.h` 默认启用该项。仅修改主题资产不保证已保存控件和动态控件都变为 10。
4. **Slate 仍需本地覆盖。** `GamePlatformDebugClient.cpp` 的自有标题、搜索和按钮使用默认字体，尚无 10 号显式保证。第三方图编辑器使用引擎 GraphEditor 控件；不得以全局修改引擎字体、节点评论字号或用户图数据来代替插件自身界面整改。
5. **资源文件引用缺口。** 见机器清单；这些仅是当前磁盘检查，不从缺文件推断其他功能状态。

## 逐插件关系表

| 插件 | 功能 | 案例 | Hub | 未指定地图 | 独立页文件 | 非 Hub 地图 |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| BlueprintAutoLayout | 5 | 5 | 5 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAArena | 5 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAClient | 26 | 2 | 0 | 0 | 存在 | /Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview |
| DBAContentPack_Common | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAFoliagePack_Core | 3 | 2 | 2 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAFrontEndPack | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAGameplay | 8 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Boar | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Dog | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Dragon | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Goat | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Horse | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Monkey | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Ox | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Rabbit | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Rat | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Rooster | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Snake | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAHeroPack_Tiger | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAPresentationPack_Core | 6 | 5 | 5 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBASFXPack_Core | 3 | 2 | 2 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAServer | 5 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAUIPack_Core | 5 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAWorldPack_MainArena | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAWorldPack_Village | 4 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| DBAWorlds | 5 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformAI | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformAbilitySystem | 4 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformAnimation | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformApplicationFlow | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformArena | 8 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformCamera | 2 | 2 | 2 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformCharacter | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformCombat | 5 | 5 | 5 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformCommerceUI | 4 | 4 | 0 | 4 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformCore | 3 | 5 | 3 | 2 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformData | 4 | 5 | 2 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformDebug | 2 | 2 | 2 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformDeveloperTools | 4 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformEntitlement | 2 | 2 | 0 | 2 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformEquipment | 4 | 4 | 0 | 4 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformGameplay | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformInput | 6 | 6 | 0 | 6 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformInteraction | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformInventory | 4 | 4 | 0 | 4 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformLiveOps | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformLoading | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformLobby | 0 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformLocalization | 0 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformNavigation | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformOnline | 7 | 7 | 0 | 7 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformPCG | 4 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformPresentation | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformProgression | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformQuest | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformSFX | 4 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformSave | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformServer | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformSession | 3 | 3 | 0 | 3 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformSettings | 4 | 4 | 0 | 4 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformSurface | 4 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformTelemetry | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformUI | 7 | 7 | 7 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformVFX | 5 | 5 | 4 | 0 | 存在 | /Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview |
| GamePlatformVillage | 0 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformWater | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformWeather | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GamePlatformWorld | 3 | 3 | 3 | 0 | 存在 | 待原生检查场景/按需指定 |
| GlooPrint | 4 | 4 | 4 | 0 | 存在 | 待原生检查场景/按需指定 |
| MobaPresentation | 5 | 0 | 0 | 0 | 存在 | 待原生检查场景/按需指定 |

## 证据与下一阶段

机器清单：`Saved/Validation/PluginSceneFontAudit/20261030/Inventory.json`，包含 70 项独立页关系、开发地图列表、WBP 文件名候选及缺失引用。当前 Monolith 明确返回 UE 编辑器未运行；未修改任何原生资产、运行源码或现行配置。

实施按同目录 `PluginSceneFontDesign_20261030.md` 执行；所有效果场景须通过原生加载、操作、停止/取消与返回核验，所有字体须回读有效 TextStyle/Font、编译保存后独立重载。构建、Cook/Stage、运行和人工签收分别记账。
