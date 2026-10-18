# SSLM音频资源库第二阶段：客户端事件接线与人工验收 V1.0

日期：2026-10-18。工程：`Game/DivineBeastsArena.uproject`（UE5.8）。本任务属于 `DivineBeasts`（项目层）纯客户端表现，未改动服务器权威AI/伤害/地图、没有复制GamePlatformSFX播放器，不新建GamePlatformAudio空壳插件。

## 一、实际资产与目录

首批9条音频定义和第二批32条CC0音频资源均保持各自真实所有者 `DBASFXPack_Core`。第二批来源与SHA见 `DBASFXPack_Core/Docs/SSLM_AudioLibraryExpansion_20261018.json`。

原生uasset清单可通过 `Tools/Unreal/Audio/ValidateAudioLibraryUE58.py`（编辑器冷重载）及 `Tools/Unreal/Audio/TestAudioLibraryStructure.ps1`（结构和指纹）复核，不能使用文本文件冒充原生资产。

## 二、运行时三层路由

```text
第三层 SSLM客户端/动画/GAS复制事实
    → UDivineBeastsPresentationClientSubsystem（项目表现协调）
    → FDivineBeastsPresentationProjectCatalog::BuildAudioLibraryFragment（按类音频目录）
    → GamePlatformPresentation（统一中立语义）
    → GamePlatformSFX（现有客户端音效执行器）
    → GamePlatformData（Definition/SFXRuntime世界租约）
    → UE原生 SoundWave、SoundConcurrency、SoundAttenuation
```

新增目录由 `DivineBeastsAudioLibraryCatalog.cpp`（项目音频库语义注册）定义，32条标签按8个类别分包。真实`ActivateContentPack`（内容包激活事务）完成全部Definition加载并注册后，分类才返回`Active`（可用状态）；`Loading`（加载中）不等于可播放。失败只导致声音不播放，不改变游戏权威结果。

`UDivineBeastsPresentationClientSubsystem::EnsureAudioLibraryForWorld`（场景音频预载）在本地玩家初始化、切图、世界上下文变化和账号切换后执行。前端只预载UI/Spells，Village预载8类；World结束、账号切换或服务析构必须先取消自有循环音，再撤销分类目录及Data租约。

注意：按当前项目现有的地图命名，Village识别保留 `GetMapName().Contains("Village")` 的兼容路径；今后需统一升级为正式WorldDefinition/Experience配置匹配，不能据此推断所有地图都已经完成声音布置。

## 三、开发与蓝图可用的接口

`ActivateAudioLibraryCategory(Category, OutError)`（按分类加载）只接受已有8类，不接受客户端请求指定原始音频路径。

`GetAudioLibraryCategoryState(Category)`（查询加载状态）可驱动人工验收UI状态显示。

`PlayAudioLibraryClip(ClipId, Location)`（按允许列表播放一次）接受已登记的32种受控标识，如：

- `UI.BookFlip`（翻书）。
- `UI.Coins`（金币）。
- `Spell.Healing`（治疗）。
- `Spell.ArcaneCast`（奥术起手）。
- `Creature.Bark`（狼吠）。
- `Footstep.LeavesA`（落叶脚步）。
- `Water.SplashA`（水面飞溅）。

`PlayAudioLibraryFactClip(ClipId, Location, OccurrenceId)`（复制事实音效请求）提供稳定身份去重，内部使用同一平台SFX服务，不由声音决定技能结果。

`StartAudioLibraryLoop(AnchorId, ClipId, Location)` 与 `StopAudioLibraryLoop(AnchorId)`（创建/释放有主人的世界循环声）只支持 `Ambience.WaterStreamA` 和 `Ambience.WaterStreamB` 两种资产原清单声明了Looping的循环声。不同锚点互不覆盖，同锚点同声音/位置重复Start不叠加，场景退出必须显式停止。锚点位置应从真实客户端场景Actor取得，不要直接录入未经实测的坐标。

**注意**：这些是项目客户端C++/蓝图调用入口；它们不等于新建UI音频审核Widget，SSLM的UI资产制作仍应通过既定Monolith MCP和人工验收。

## 四、当前实际接线

1. **狼与吸血怪物攻击**：`DivineBeastsVillageWolfAnimInstance`（客户端怪物动画实例）根据已复制的`FGamePlatformAbilityCastFacts`（施法事实）及Montage成功启动后触发狼吠/重挥击；使用`ActivationId`（本次动作身份）消除重复请求。
2. **狼死亡**：只在本地客户端观察到从“存活”变化为“死亡”的状态时触发低吼；首次进入已死亡角色不会重放死亡声音。真正伤害/死亡仍由服务器处理。
3. **治疗技能**：`HandleDevelopmentHealingFeedback`（确认治疗事实适配）在既有允许列表校验后，用`EventId`触发`Spell.Healing`；视觉VFX另走原提供者。
4. **奥术飞弹**：根据批准的真实客户端弹道启动事件添加`Spell.ArcaneCast`声音，利用`OccurrenceId`去重。它只是当前开发法术原型，不是完整十二生肖技能分层音频。
5. **脚步通知**：新增`UDivineBeastsAudioFootstepNotify`（动画脚步通知），在骨骼落脚帧进行一次射线追踪，优先用`PhysicalMaterialToClip`（物理材质名称→音效语义）映射，缺失则使用`DefaultClipId`（默认脚步）。它**尚未批量写入所有角色动画资源的Notify轨道**，必须在真实脚步着地帧由UE编辑器逐段配置后才能自动播放，不能说实际全角色脚步已经启用。
6. **环境循环**：已提供真实三维水流播放/取消接口，但`L_Village_Start`中实际声源Actor/区域映射与原生试听未由本阶段完成；无固定声源位置不得自动伪造瀑布声音。

## 五、编辑器手动验证建议

1. 在UE5.8 Content Browser（内容浏览器）开启Show Plugin Content（显示插件内容），打开`/DBASFXPack_Core/Audio/Library`，抽样试听32份Waves与Definitions，并核对3D衰减、循环无爆音。
2. 通过Editor Details（细节面板）在合法客户端角色行走动画落脚帧添加`SSLM Audio Footstep`（项目脚步通知）；例如给石面对应PhysicalMaterial配置`Footstep.Stone`，落叶对应`Footstep.LeavesA/B`，不要写入服务端权威动画。
3. 运行真实Village客户端，狼正常攻击、死亡阶段检查对应声音；重复复制同一次`ActivationId`不得出现两声。
4. 使用12生肖占位治疗/奥术飞弹，真实事件触发时应有对应音效；无音频分类Active时应安全静音、不得影响技能伤害或回血。
5. 创建专属客户端声音锚点，对真实水流场景Actor调用Start/StopAudioLibraryLoop；确认临近音量与离开停止。
6. 运行双客户端、断线重连、进入/退出World、5v5压力场景，用Audio Insights测量音频线程/并发声音；用Server Cook结果证明客户端纯音频资源没有入包。

## 六、测试与状态

- `Tools/Unreal/Audio/TestAudioLibraryBindings.ps1`：32语义、客户端预载、狼/治疗/奥术/脚步接线静态门禁。
- `Tools/Unreal/Audio/TestAudioLibraryStructure.ps1`：32个素材/定义、来源SHA/License等静态门禁。
- 32条声音映射/生命周期/狼与法术事件/脚步通知静态回归：`SSLM_AUDIO_LIBRARY_BINDINGS_STATIC_OK 32 categories=8 wolf=1 spells=2 footstep_notify=1`，通过。
- 32份原始音源及原生资源结构：`SSLM_AUDIO_LIBRARY_STATIC_VERIFY_OK 32 4`，通过。
- 首次真正UE5.8 Editor模块增量构建：`Build/Game/BuildFoundation.ps1 -Editor -UsePrecompiled -NoSharedPCH -NoUBA -MaxParallelActions 8 -OnlyModules DivineBeastsPresentationRuntime,DivineBeastsPresentationClient`，退出码0。日志目录：`Saved/Validation/FoundationM0/dc977036-dddb-4ffd-aa4c-76728c9c4339/Build`。此为模块级构建，不是完整Editor/Client/Server目标，更不等于玩家听到音频。
- 后续新增`Private/Tests/DivineBeastsAudioLibraryCatalogNativeTests.cpp`（32条类别语义原生自动化测试）。首次增量构建因其他Foundation构建或Cook持锁返回2=`NotExecuted`；再次尝试已编译该.cpp对象，但编辑器现有进程锁住模块DLL，链接返回LNK1104。详情参见文末‘实际自动化与构建证据’。
- 新增自动化测试正式名称：`DivineBeasts.Audio.Library.CategoryCatalog`，须在锁释放后按现有构建参数增量编译，并在Unreal Editor Automation框架中运行，检查8类条目数量、32种身份唯一性及未知分类安全回退。
- 真实手工听音、动画资产Notifies、地图声源实际布置、Cook、5v5性能数据仍为独立验收项，未执行不得标记完成。

## 七、回退与升级

本轮只新增项目表现源码和按分类映射，没有修改原32份音频uasset，不改原9份首批播放流程。若实际测试不佳，可按场景停用本项目新音频分类预载和调用；播放系统仍沿用`GamePlatformSFX`原路径。将来切换为正式Music Director（音乐导演）不应触动SFX Definition逻辑身份，也不应重新实现一个并行SFX播放器。

## 八、2026-10-18续做：独立UE审核地图和脚步动画原生资产

### 音频审核地图（真实UE生成，并已独立冷重载）

- 通过 `Tools/Unreal/Audio/AuthorAudioLibraryReviewMap.py`（审核地图制作器）由UE Editor真正创建 `/Game/Development/Audio/L_SSLM_AudioLibrary_Review`（开发专用音频试听地图）。
- 地图含12个 AmbientSound（声音Actor）：两条水流、鸟鸣、落叶/石面脚步、狼嚎/狼吠、奥术/治疗/传送、兵器碰撞、UI确认。原生AudioComponent使用真实SoundWave；水流A自动循环，其余供人工按需试听。
- 制作日志 `Game/Saved/Logs/SSLM_AudioReviewMapAuthor_20261018.log`，标记 `SSLM_AUDIO_REVIEW_MAP_OK /Game/Development/Audio/L_SSLM_AudioLibrary_Review 12`。
- `Tools/Unreal/Audio/ValidateAudioReviewMapUE58.py`（独立冷重载脚本）重新打开同一地图，12份声音与1个自动循环标记全部回读成功；日志 `Game/Saved/Logs/SSLM_AudioReviewColdVerify_20261018.log`，标记 `SSLM_AUDIO_REVIEW_COLD_VERIFY_OK 12 1`。
- 该图不是GamePlatformPresentation运行链的真实联机验收，不进入正式地图，也不表示已通过扬声器试听或声学人工签字。它是独立的音频内容审核载体。

### 脚步通知动画审核副本（正式动作未被修改）

- 真实源动画 `/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Walk_Fwd`（走）和 `MF_Unarmed_Jog_Fwd`（慢跑）当前均无脚步Notify事件。
- 仅通过引擎API复制到 `/Game/Development/Audio/Animations/AS_SSLM_AudioReview_Walk` 和 `AS_SSLM_AudioReview_Jog`，分别加入左右脚两次 `UDivineBeastsAudioFootstepNotify`（自定义落脚音频通知）。
- 审核时间点仅为参考：Walk约0.33/1.05秒、Jog约0.353/1.201秒，默认Footstep.LeavesA/B；因原动画脚骨采样未产生可信时变高度，不能当成实测精准着地时间。必须在动画编辑器看真实足部运动，并按已审核地表物理材质微调后才能迁入正式动作。
- 制作日志 `Game/Saved/Logs/SSLM_FootstepAudioReviewAuthor_20261018.log`，冷回读 `Game/Saved/Logs/SSLM_FootstepAudioReviewColdVerify_20261018.log`，证据 `SSLM_FOOTSTEP_REVIEW_COLD_VERIFY_OK 2 4`；同时确认正式两段动画未增加本次审核轨道。
- 验证工具：`Tools/Unreal/Audio/AuthorFootstepAudioReviewAnimations.py`、`Tools/Unreal/Audio/ValidateFootstepReviewAnimationsUE58.py`。

### 新手村实地图只读检查

使用独立UE5.8编辑器加载 `/DBAWorldPack_Village/Maps/L_Village_Start` 并枚举本次载入的Actor：共10个，未检出已命名或Tag标注为瀑布/湖泊/溪流/桃林/鸟群的声源锚点。证据 `Game/Saved/Logs/SSLM_VillageAudioAnchorInspection_20261018.log`。该结果只描述**本次实际加载的Actor**，不代表将来PCG烘焙或World Partition其他分区没有声音对象。无真实位置时不填入虚构的瀑布声源坐标；正式落图需场景作者提供或原生AssetRegistry确认可用锚点后，再调用`StartAudioLibraryLoop`。

### 实际自动化与构建证据

- 受影响两个客户端模块在 `DivineBeastsArenaClient`（客户端目标）的`OnlyModules`增量编译成功，退出码0：`Saved/Validation/FoundationM0/65f67d32-bf38-4306-aef0-aad66910b9ff/Build`。该证据是Client模块级编译，并非完整Client发布构建。
- 编辑器中新添的`DivineBeastsAudioLibraryCatalogNativeTests.cpp`（原生分类测试）曾因锁返回NotExecuted；随后重新编译其C++对象成功，但DLL链接失败`LNK1104`，源因已有UE Editor进程占用`UnrealEditor-DivineBeastsPresentationRuntime.dll`。详细日志 `Saved/Validation/FoundationM0/a28e34b9-b8cc-471e-baac-daef243ace39/Build/Editor/UBT.log`。不得把对象成功编译当成Editor模块链接或自动化测试已运行。
- 使用既有Editor可加载的音效模块，独立原生Automation实际运行`GamePlatform.SFX`前缀5项，全部Success；证据 `Game/Saved/Validation/SSLM_AudioPlatformSFXNative_20261018/Report/index.json`。这5项仅证明平台SFX原有策略和生命周期测试，**不替代新32项Project Catalog原生测试**。
- 尚需在不抢占其他任务编辑器资源的条件下，关闭正在使用该DLL的原有Editor（由对应持有者操作），按锁定UE5.8构建基线重新链接Editor模块并运行 `DivineBeasts.Audio.Library.CategoryCatalog`。
- 本轮尚未完成带真实声音输出的PIE试听、正式动画脚步帧签字、正式新手村水体声源落图、客户端Cook/服务器隔离与5v5 Audio Insights压测。

