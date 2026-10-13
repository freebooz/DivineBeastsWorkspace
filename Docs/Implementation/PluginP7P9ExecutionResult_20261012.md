# 《神兽联盟》第三批 P7–P9 实施与交付验收记录（2026-10-12）

工程：`DivineBeastsWorkspace（神兽联盟工作空间）`；引擎：UE5.8；审查基线：Git `ecb9d85617947b83996dafd8914fab15e8c252e2`；执行状态：**本批技术子集有真实通过证据，全部65个插件及人工/美术验收仍未完成。**

> 本文分别标记“源码存在、UE资产有效、自动化测试成功、真实场景运行、人工签收”，不将任何一项外推为其他项已完成；不重写其他并发任务中的正式UI、生肖定义及地图。

## 1. 本批形成的实际工程资产

### P7 表现、音效、天气、PCG

- 既有 `GamePlatformSFX（通用音效）` 单一SFX执行器已提供 `AGamePlatformSFXReviewActor（音效审核演员）`；新增项目内容包 `DBASFXPack_Core（独立音效资源包）` 已登记。真正的UE资源有3个SoundWave（小雨、大雨、风）和2个GamePlatformSFXDefinition（天气声音定义）。音频属于原创合成原型，不能充当完成的人耳品质验收。
- `/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview`（声音审核地图）中有2D、World（世界空间3D）和Attached（附着）三类真实播放站；实际PIE报告 `SFXRuntime/SFXRuntimeReview.json` 状态Passed，5项检查含三条真实AudioComponent处于PLAYING、请求拒绝不污染、停止清理、宿主销毁、世界清理。**它是运行状态验证，不证明听觉质量。**
- 鸟群、Fauna（动物生态）、自然落叶、Weather（天气）、Snow（雪）、PCG Gold等继续复用既有地图和Player/Visual/PCG原生测试，不新建重复VFX或PCG播放器；PCG修复了旧自动化中数组自引用导致断言的测试夹具，保留生产契约本身。
- 当前 `CompleteEditorNative/index.json` 中114项原生专项测试全部Success，其中5例有Warning，0 Fail；包含PCG、VFX、Weather、Surface、SFX、UI及玩家服务的本批覆盖范围。
- `DBASFXPack_Core` 只在Client/Editor装配；专用Server排除纯音效内容。

### P8 玩家服务及十二生肖

- Entitlement（权益）、Equipment（装备）、Progression（成长）、Inventory（背包）、LiveOps（运营）、CommerceUI（商业界面）的一部分客户端/服务器合约纳入真实原生专项测试；Go沙箱回归3项通过，未接触生产支付或真实用户余额，不能推为全业务后端闭环。
- `/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase`（12生肖共享展示图）保存12个 `GamePlatformCharacterPreviewStage（平台角色预览舞台）`，各自引用已有英雄Appearance、原型材质和Manny/Quinn骨骼网格，真实UE资产回读12/12正确。12套仍是原型，不是最终生肖3A网格、动画及完整技能。
- 在上述英雄图真实UE5.8创建 `ReviewHeroCloseup_Rat/Ox/Tiger/Rabbit/Dragon/Snake/Horse/Goat/Monkey/Rooster/Dog/Boar` 共12个CameraActor，供逐个选择Pilot（驾驶摄像机）查看材质、轮廓、骨骼。保留原总览摄像机。
- 在声音图创建 `ReviewSFXCloseup_TwoD/World/Attached` 共3个CameraActor，供人工选择试听站；原3个SFX Actor和其2D/世界/附着定义完全保留，不因新增相机自动发声。
- 两张地图修改前原件和SHA备份保存在 `Saved/Validation/PluginP7P9_20261012/ReviewCameraBackups`。全部15个新相机通过 **两个独立UE5.8编辑器进程重新打开验证**，引擎进程零退出错误；保存的Asset未以文本伪造。
- `Visual/Closeups/heroes_Rat.png` 是真实1920×1080 UE5.8 RHI 近景截图，`Visual/Closeups/heroes_Rat.json` 记录 `captured`，图像SHA-256：`a25975073ccfc4eb65ad0156f0ede9a293ea47a9080e469f0762fbbd38eaf519`。这张图看到公共白色人形原型的侧面，背景其他英雄标签干扰，**不符合正式美术展示终稿**。自动化不能代替真人审美签收。

### P9 统一交付证据

- `/Game/Development/Foundation/PluginReviews/Cases/RC_<PluginName>` 现有65个 `UGamePlatformReviewCase（审查用例资产）`，与47个代码插件、18个内容包逐一对应。旧64插件统计已随真实音效包增加到65，不恢复已经退休的旧代码插件。
- 修正 `Build/Validation/CollectP7P9Delivery.py（统一证据汇总器）` 把已验证包内运行结果误读为旧目录的缺陷：必须读取 `PackagedRuntimeVerified/Summary.json` 并关联其中的真实 `index.json`、同RunId打包客户端EXE；缺报告、重复/失败/空测试、无效路径或旧客户端均不能标记通过。
- 新增 `Tests/Assets/TestP7P9PackagedEvidence.py`：13项回归 **先红灯（函数缺失），修复后13/13通过**。`TestP7P9Composition.py` 10项、`TestPluginDeliveryEvidence.py` 37项、`TestCharacterCreationIntroductionSources.py` 11项亦已真实运行通过；本轮额外静态单元测试总计 **71/71通过**。
- 当前源代码结构门禁 `Tests/Architecture/ValidateDesignBaseline.ps1` 实际退出0；命名门禁 `Tests/Architecture/TestEnglishProjectNames.ps1` 检查 **143,918项 / 65插件，0违规**。
- `Build/Validation/CollectP7P9Delivery.py` 以现有真正工作区输入再次运行成功；输出 `PluginDeliveryCatalog.json（逐插件交付台账）`、`PluginReviewIndex.md（逐插件审核索引）`。汇总明确 `technicalDeliveryVerified=true`、`allPluginsComplete=false`、`humanAcceptance=pending`。

## 2. 真实构建、发布和运行边界

| 类别 | 当前报告及结果 | 不得外推的范围 |
|---|---|---|
| 编辑器专项原生测试 | `CompleteEditorNative/index.json`：114/114 Success，5项带Warning | 不是所有65插件功能均通过 |
| 真实声音生命周期 | `SFXRuntime/SFXRuntimeReview.json`：5项真实PIE检查通过 | 不是人耳音质、频谱、设备验收 |
| 两个正式构建目标 | `FinalTargetBuilds.json`：Client、Server分别退出码0 | 不等于所有地图及UI场景联机通过 |
| 客户端新版容器 | 当前最终ClientContainer.csv：正式音效依赖4项齐全、音效包文件6项、总1339文件、开发审核地图0 | 不证明最终天气听觉 |
| 服务器新版容器 | 最终ServerContainer.csv：音效包文件0项、总646文件 | 不代表所有服务端业务逻辑成功 |
| 打包客户端原生回归 | `PackagedRuntimeVerified/index.json`：3/3成功，1项有Warning，与当前Cook运行编号 `da53c17e-706f-44ff-90ac-27064b392199` 的可执行程序匹配 | 仅三项基础逻辑，不是登录-竞技或实机音频 |
| Go后台沙箱 | `BackendReview.json`：3项实际Go测试成功，生产事务未调用 | 不涉及真实支付、账本、订单闭环 |
| UE真实资产复核 | `Validate_assets.json`：18内容包674个uasset加载成功，65个ReviewCase有效 | 不保证最终场景表现及全部资源制作完成 |
| UI/英雄/SFX开发图 | `Validate_heroes.json` 和 `Validate_sfx.json` 原审查通过；新增15相机独立重载通过 | 美术、音质、性能人工结论待定 |

输入SHA、真实目标和开发资源排除条件来自已有工程验证目录；不将开发审核地图放入Shipping烘焙。不对因其他项目会话持续修改的12生肖正式Hero定义、前端Widget、桃花Niagara、DefaultGame.ini做未获授权的替换。

## 3. 65插件审核门禁的真实汇总

`Saved/Validation/PluginP7P9_20261012/PluginDeliveryCatalog.json` 逐项给出G0至G5及对应证据。

- G0：当前工程结构检查65插件通过，不能推出业务完整。
- G1：本批受影响编辑器模块和Client/Server编译有真实报告，但**其他插件不能无条件标记新编译通过**。
- G2：18内容包的674件资产真实加载并保持SHA一致，部分代码插件相关蓝图/地图仍需独立重载。
- G3：本批逐插件统计 **14个插件存在已执行通过的专项原生测试**；18个纯内容包“不适用代码单测”；**33个代码插件在本批没有完整原生验证**，应维持NotRun，不能作为全量完成。
- G4：现有专项地图、近景观察相机和运行时组件可供人工使用；真实性能、完整天气/PCG/MOBA/全部VFX效果未全部签收。
- G5：新版Client/Server容器中的声音依赖及排除检查有真实通过证据；但人工最终签收和全功能发布验收未完成。

**因此：本批范围内技术子集已验证；65插件全量R4生产交付仍未达到。**

## 4. 人工验证操作清单（由审核人填写，不自动代签）

### A. 十二生肖视觉（P8）

在UE编辑器打开：`/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase`。
在Outliner（场景大纲）搜索 `ReviewHeroCloseup_`，找到指定近景相机后 Pilot（驾驶视角）。按 `Rat/Ox/Tiger/Rabbit/Dragon/Snake/Horse/Goat/Monkey/Rooster/Dog/Boar` 对应子鼠、丑牛、寅虎、卯兔、辰龙、巳蛇、午马、未羊、申猴、酉鸡、戌狗、亥猪，逐项确认模型、材质、是否反面/穿模、形体比例、光照、表情、动画和第一眼生肖辨识。

当前截图显示原型模型为公共人形且侧面视角中其他展台文字穿过背景；人工应明确勾选“仅原型通过”或“美术需返工”，不得填“最终3A通过”。

### B. 音效试听（P7）

打开 `/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview`，Outliner搜索 `ReviewSFXCloseup_` 选择2D/World/Attached站点。进入独立开发PIE或客户端音频会话，显式调用站点 `PlayReview（试听）`／`StopReview（停止）`，确认播放音量、距离衰减、附着位置变化、同时播放上限、循环边界与结束清理。关闭全局静音，至少在耳机和目标扬声器上试听，记录设备、系统音量、时间及感受；当前三条音频为合成原型，不自动通过品质验收。

### C. 其他表现与玩家服务（P7–P9）

- 复用现有鸟群、Fauna、落叶、雨雪、地表材质、PCG Gold审核地图，分别检查真实形态、效果强弱、可见距离、性能、流送和世界退出。
- UI十大域在PC和适用移动端确认视觉、响应式布局、输入、焦点和可访问性；UI资产只能通过Monolith能力编辑。
- 玩家服务使用开发沙箱核对背包/装备/成长/权益/运营/商业界面的一致性和幂等；不在生产环境创建订单或修改余额。
- 发现错误时记真实现象/日志/截图，状态为Fail并关联任务；未验证留NotRun，不能默认为Passed。

审核单应由实际审核人提供：**审查者、设备/运行环境、版本/RunId、每个域的真实截图或试听证据、通过或驳回结论、日期**。本轮所有自动报告均保留 `manualAcceptance=pending`。

## 5. 证据和回退

- 总目录：`Saved/Validation/PluginP7P9_20261012`。
- 统一台账：`PluginDeliveryCatalog.json`、`PluginReviewIndex.md`。
- 原生测试：`CompleteEditorNative/index.json`、`PackagedRuntimeVerified/index.json`、`CurrentPolicyTests.json`。
- 新增测试：`Tests/Assets/TestP7P9PackagedEvidence.py`；真实UE资产改进：`Tools/Unreal/PluginReviews/ExtendP7P9ReviewCameras.py`、`CaptureP7P9Closeup.py`。
- 相机回读：`ReviewCameras_heroes_inspect.json`（12）与 `ReviewCameras_sfx_inspect.json`（3）；近景截图 `Visual/Closeups/heroes_Rat.png`。
- 编辑前 .umap 数据备份：`ReviewCameraBackups/` 下两地图按SHA命名的历史字节副本；当前游戏运行规则没有改动，所加CameraActor只属于Development/PluginReviews。
- 所有本轮改动均保留在Git工作区，由正式工程负责人审查后合并；未擅自重置其他任务的已改文件、没有自动提交/推送。

**下一步优先事项：** (1) UI/天气/地表/PCG在目标设备的真实人工验收；(2) 生肖专属3D模型、专属动画和技能特效代替Mannequin原型；(3) 33个未完整覆盖的插件按适用目标补独立测试或批准不适用结论；(4) P5–P6真实竞技、服务端准入及登录全链仍需继续单独验收。人工未签之前不能将P7–P9标记全部完成。
