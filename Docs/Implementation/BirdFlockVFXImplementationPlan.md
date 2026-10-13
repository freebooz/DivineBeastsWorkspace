# BirdFlock VFX（通用鸟群视觉特效）实施执行计划

版本：V1.0｜2026-10-11｜工程：DivineBeastsWorkspace（SSLM工作空间）｜状态：执行中

## 1. 目标和非目标

- 统一鸟群环境视觉特效：鸟类类型（物种资源引用）与数量（整数）在数据资产/蓝图实例中可设置，编辑器可修改并运行验证。
- 初始物种：Seagull（海鸥）、Swallow（燕子）、Eagle（雄鹰）。鸟群数量配置合法范围 0–512，物种可以使用独立配置进行更低上限；0 必须关闭实例，不能保留隐形模拟。
- 飞行表现：环绕、定向航行、聚散、风场扰动、滑翔、随机相位振翅、过度倾斜保护。行为和外观解耦，同一机制复用。
- 纯环境表现是客户端本地视觉，无服务器权威移动、碰撞、伤害或额外网络复制；不为每只鸟生成独立 Actor 或 SkeletalMeshComponent。
- **边界**：平台层不包含项目鸟种贴图/模型/ID；MobaCommon 不承载普通自然环境；不创建新 VFX 同领域插件/播放器；不绕过 GamePlatformPresentation（表现分发）与 GamePlatformData（资源租约）的正式定义链。
- 首期优先基于最小可验证网格与共享材质完成飞行样机；需要真实飞行动画的近景资产可以后续替换 VAT（顶点动画纹理）/骨骼烘焙资源。不能把待替换的低面数原型宣称为3A最终美术。

## 2. 目录与分层

平台层现有 Game/Plugins/GamePlatform/Presentation/GamePlatformVFX：
- Content/Environment/BirdFlock/{Materials,Niagara,EffectTypes}：中立母材质、Niagara 基础模块、特效类型。
- Source/GamePlatformVFXClient/{Public,Private}：通用鸟群配置校验、飞行控制及现有 VFX 执行器扩展（仅必要时）。
- Source/GamePlatformVFXEditor：资产及参数校验复用现有 Editor，不新增空模块。
- Docs/BirdFlock.md：参数、效果实现、制作与验证说明。

项目层现有 Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core：
- SourceArt/BirdFlock/{Meshes,Textures}：可再生成的 Blender 源、FBX 模型及 PNG 原始纹理。
- Content/VFX/Environment/BirdFlock/{Meshes,Textures,Materials,Niagara,Definitions,Profiles}：真实 UE 资产，只能由 UE 工具创建。
- 不向 MobaCommon 增加自然环境行为与硬依赖。后续项目世界场景通过 DefinitionId（特效定义身份）引用相关项目资产。

## 3. 参数协议

- Species（鸟类类型）：优先使用类型无关的资源配置 DataAsset/软引用，通过平台定义逻辑 ID 选择项目物种；不在平台层写死 Seagull/Swallow/Eagle。
- BirdCount（鸟类数量）：int32，用户配置优先，最小0、硬限制512，资源类型与画质可继续限制；请求错误时不截断为意外的大群，不允许负数。
- RadiusCm（盘旋半径）、AltitudeCm（相对飞行高度）、CruiseSpeedCmPerSec（巡航速度）、WingBeatHz（振翅频率）、WindInfluence（风力影响强度）、Seed（随机种子）、FlightMode（飞行模式）。
- Niagara 可识别 User.BirdCount / User.FlockRadius / User.FlockHeight / User.FlightSpeed / User.WingBeatHz / User.WindInfluence 等稳定参数；参数类型、范围、初始化与取消遵循现有 VFX Definition Schema。
- 配置持有初始值；实例覆盖优先级高于预设，随后经场景距离/画质预算门禁得到 EffectiveCount（实际可视数量），与 RequestedCount（原始请求数量）分开统计。

## 4. 资源生产顺序（必须）

P0：只读审查实际依赖、用户改动和编辑器，形成本计划。完成标准：定位插件/脚本/引擎和受影响路径。
P1：先生成 Blender 参数化三鸟低面数网格与 UV、顶点色翼部权重，纹理原图和清单（包含哈希）；生成图不作为已有 .uasset 的替代。完成标准：FBX/PNG 能被读取，网格轮廓合理且不同物种可辨。
P2：以锁定 UE5.8 官方编辑器导入 FBX/PNG，创建平台级母材质（无项目资源硬引用），项目材质实例。完成标准：真实 .uasset 保存并重新加载。
P3：实现通用飞行行为/数量边界和材质相位等算法；优先用现有 Niagara 管线和参数，不创建第二套 VFX 服务。
P4：创建并保存 Niagara System/Emitter、Niagara EffectType、项目鸟种 DataAsset/Definition、复用的蓝图入口或编辑器演示 Actor；无实装则如实标为缺口。
P5：单独生成鸟群视觉审核关卡，完成后再以不覆盖并发修改为前提接入新手村、开放世界和角色选择/创建的表现配置。
P6：用已安装 UE5.8 编译模块/工程，执行资源回读、单元测试、Niagara 编译和客户端 Cook／服务器排除核查；不可执行项留日志并说明。
P7：人工编辑器验收：鸟种切换、数量0/1/32/128/512、不同随机种子、10分钟运行、世界退出清理、过渡平滑、无穿模/闪烁/异常锯齿。人工未操作不得标记通过。

## 5. 门禁与并发保护

- 修改前记录 git status，保护既有 dirty 文件；新增独立 BirdFlock 子树，禁止覆盖正在修改的 FallingFoliage 资源、UI、角色选择地图。
- UE资源必须通过真实 UE AssetTools / 编辑器创建，失败时不创建伪 .uasset。
- Blender/原始 PNG/FBX 属 SourceArt；引擎二进制资产属于 Content；暂存日志与生成报告属于 Saved。
- 中文注释、ASCII源码和资产文件名、对外字段单位、客户端专属目标规则必须遵守 AGENTS.md 和 Game/Plugins/插件开发规范.md。
- 编译、自动化检查、Cook、运行截图及人工验收分别留下证据，不得将局部成功上推为全功能完成。

## 6. 人工验收检查清单

- [ ] 海鸥、燕子、雄鹰分别在编辑器可选择、可保存/重载。
- [ ] 数量0、1、32、128、512下系统稳定，0无可见实例，无无限制生成。
- [ ] 盘旋、定向飞行、随机振翅与风场变化可观察，单鸟不发生突然反向或大幅翻转。
- [ ] 鸟群与相机/地形空间关系正常，无穿过地面、水面或建筑物的严重问题。
- [ ] 新手村、开放世界及选角/创建界面接入内容合理、不干扰交互。
- [ ] Client/Editor 构建通过；Dedicated Server 不加载纯视觉资产；Cook 和性能预算通过。
- [ ] 真机近远景性能以及 Niagara Debugger 统计由人工检查并存档。

## 7. 风险

已有 UE 编辑器和其他自动化任务处于活动状态。若无法安全地生成真实 Niagara/地图资产，不修改别人的场景或工作流；先保留已验证纹理/网格/算法/工具，并把未通过的里程碑明确列为阻塞，不声称完成。

## 8. 当前P4–P7续执行结论（2026-10-11）

P4已完成真实GPU Niagara模板、三种系统与WorldDefinition、统一场景蓝图；P5已通过正式场景蓝图→Presentation→Data租约→VFX→Niagara运行集成，数量/类型切换及世界清理均经实际GPU读回验证。P6已完成受影响Editor模块编译、完整Client/Server构建、3项客户端自动化、15组GPU数量、15组正式链路、600.657秒稳定性61次抽查、Client/Server Cook/Stage和IoStore内容门禁。客户端必需鸟群资源19/19，服务器鸟群资源0项。

P7已准备真实UE图形预览、五张截图及人工验收单，但仍等待用户对生态摆放、轮廓、遮挡及目标设备性能的人工确认。全Editor引擎重建不在定向模块成功范围；原型网格未引用但仍有7项被旧AlwaysCook纳入，保留为后续包体清理项。

最新真实路径、失败修复、验证报告、发行目录和范围见`BirdFlockP4P7Execution_20261011.md`；人工步骤见`BirdFlockManualAcceptance.md`。以下第9节保留此前P0–P3历史记录，不作为当前状态。

## 9. P0–P3历史执行记录（已由第8节续执行结论更新）

> 2026-10-12清理注记：下列历史记录中出现的旧ISM作者 `AuthorBirdFlockProfilesAndBlueprint.py` 已退役清理，不是现行可执行入口。当前定义/场景蓝图使用 `AuthorBirdFlockDefinitions.py` 与 `AuthorBirdFlockSceneBlueprint.py`，地图使用 `AuthorBirdFlockNiagaraReviewMap.py`／`AuthorBirdFlockIntegrationReview.py`；已弃用的旧捕获与探针清单、备份和恢复依据见 `ScriptCleanup_20261012.md`。历史测试结论保留，勿按旧记录重新生成过期资产。

- [x] P0：三层架构、GamePlatformVFX接口和并发状态审查。
- [x] P1：3类鸟的低面数FBX/UV/顶点色、3张源纹理、Blender可编辑文件、SHA-256 Manifest及静态剪影预览。
- [x] P2：真实UE资产：3个厘米尺寸UStaticMesh（`*_Cm`）、3个Texture2D、1个母材质、3个材质实例；已通过UE5.8重新加载和尺寸验证。早期尺寸100倍错误的3个原型网格仍保留但不再引用，需后续受控清理。
- [x] P3（代码与无渲染设备实例生命周期测试）：通用鸟群Actor/预设、0–512数量、飞行轨迹、风扰动、ISM批量可视实例代码已完成Client编译；在真实客户端NullRHI进程中执行实际Actor实例创建、0/1/32/128/512切换、半画质256和释放成功。尚未进行GPU可见画面或人眼验收。
- [ ] P4：已编写 `AuthorBirdFlockProfilesAndBlueprint.py`，但**未运行**；真实项目DataAsset/统一蓝图和Niagara发射器/系统/Definition尚未生成。
- [ ] P5：已编写隔离审核地图作者脚本，但尚未创建.umap或修改正式Village/OpenWorld/选角地图。
- [ ] P6：Client、Server Win64 Development编译均成功，10/10真实UE资产回读通过，英文命名门禁检查115517项、64插件、0违规。另通过独立客户端真实自动化2/2：CountAndSeed（数量与散列）、ActorInstanceLifecycle（实例生命周期）。Editor编译仍被其他UE Live Coding会话阻止；Editor测试、Niagara GPU编译、Cook和性能实测未完成。
- [ ] P7：Blender静态预览已生成，UE场景人工验收未进行。

证据：`Game/Saved/Logs/BirdFlockScaleCorrection.log`、`Game/Saved/Logs/BirdFlockScaledAssetsValidation.log`（海鸥半翼宽85.30cm、燕子53.24cm、雄鹰139.50cm，10项通过）；`Game/Binaries/Win64/DivineBeastsArenaClient.exe`、`DivineBeastsArenaServer.exe` 和 `DivineBeastsArenaServer.target`（Server不包含GamePlatformVFXClient）；`Game/Saved/Automation/BirdFlockClientActor/index.json`（自动化2项Success、0项Failed），`Game/Saved/Logs/BirdFlockClientActorAutomation.log`（真实客户端执行日志）；`SourceArt/BirdFlock/Previews/BirdFlockSpeciesContactSheet.png`（Blender静态预览，不是UE截图）。

恢复条件：其他任务安全保存并结束当前UE编辑器的Live Coding会话后，重新编译 `DivineBeastsArenaEditor Win64 Development`；然后创建真实DataAsset/Blueprint/Niagara资源、审核地图与正式场景接入，执行自动化/Cook/性能与人工视觉检查。不可在编辑器锁未解除时伪造通过状态。
