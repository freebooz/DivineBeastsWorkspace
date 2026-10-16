# SSLM UE5.8 电影级摄像机 P0—P8 审查及实施记录（2026-10-15）

## 执行与证据范围

唯一仓库 freebooz/DivineBeastsWorkspace；本轮检查 main 的基线提交为 `239184c6744201d303bb5584232ee4f134ed891e`。HWebCodex执行端不可用（Tunnel-client has not been seen for 300 seconds），改由同一GitHub仓库进行只读源码审查，在隔离分支 `work/sslm-camera-cinematic-20261015` 保存可回退增量，未更改main。未能访问本地未提交文件、UE Editor、蓝图资源编辑器和构建环境，因此源码审查仅代表main已提交版本，不能代替工作目录审查。

## P0 实际定位

1. `Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/` 已存在平台相机插件，ClientOnly模块 `GamePlatformCameraClient`，并非新插件需求；当前实现 `UGamePlatformCameraHitFeedbackSubsystem`，仅有命中镜头震动本地玩家子系统及强度回归测试。早先`Docs/ImplementationSpecification.md`描述的完整模式栈、异步租约、视角转换及CameraModifier尚未形成生产代码。
2. `Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/Preview/GamePlatformCharacterPreviewStage.cpp` 创建普通 `UCameraComponent`，具备旋转和距离调整、无Actor Tick；已有的展示功能应继续复用。
3. `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Characters/DivineBeastsCharacterPreviewSubsystem.cpp` 通过 `SetViewTarget` 切换，原有切入/切出无时间插值；具有旧视角缓存及世界清理逻辑，需要验证在高级相机接入后的正确恢复。
4. `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient/Private/Subsystems/DivineBeastsInputClientSubsystem.cpp` 中`HandleMoveLookInput`为原有LookDelta/LookRate的最终消费者。若迁移到Camera服务，须原子改造而不能重复AddYawInput/AddPitchInput。
5. `Game/Plugins/MobaCommon/Presentation/MobaPresentation/MobaPresentation.uplugin` 已登记Camera插件依赖；客户端Build.cs已私有依赖GamePlatformCameraClient。 `Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin` 未显式声明Camera插件依赖；通用非竞技模式的正式加载仍未验证。
6. `Game/Config/DefaultScalability.ini` 的最低后处理质量已禁用运动模糊；尚未形成全模式景深、曝光与电影后期的数据驱动统一控制。

## P1 设计决定

保留已有GamePlatformCamera（平台）；MobaPresentation及GamePlatformArenaClient（MOBA竞技）；DBAClient/DBAArena（项目组合）。不得直接新增平台GameFoundation目录或重复CineCamera管理器。第一批仅扩展ClientOnly模块，不建立空Server模块，不更改项目输入链。以引擎CameraComponent和PlayerCameraManager为常态，以现有或预置CineCameraActor作为可选展示镜头。专用服务器不需要新增依赖。

## P2—P3 第一批实际源码

- `Public/Cinematic/GamePlatformCinematicCameraProfile.h`（客户端镜头数据配置），对应`Private/Cinematic/GamePlatformCinematicCameraProfile.cpp`的合法性校验。
- `Public/Cinematic/GamePlatformCinematicCameraSubsystem.h`及`Private/Cinematic/GamePlatformCinematicCameraSubsystem.cpp`：本地玩家作用域，针对同一世界已有Actor平滑进入与退出，核对旧目标所有权，失败不改变机位；仅对CineCameraActor下的CineCameraComponent应用光学参数，竞技模式不修改光圈和对焦。
- `Private/Tests/GamePlatformCinematicCameraProfileTests.cpp`：默认/坏焦距/坏混合时间的自动化测试源码。
- `GamePlatformCameraClient.Build.cs`：新增引擎CinematicCamera私有模块依赖。
- `AGENTS.md`（既有全局策略）追加电影机位设计与验证约束，没有创建第二个全局策略文件。
- `GamePlatformCamera/README.md` 追加本批能力及未验证边界。

上述是**源文件已写入**，并非资产、运行或编译通过。

## P4—P8 待执行

P4：角色预览工作室CineCamera接线；新手村和开放世界镜头；进入竞技场自动强制应用公平视野配置；断线重连、死亡、复活、重新附身和世界旅行恢复。须先完成完整模式管理与输入单一消费者整改。

P5：用UE5.8真实创建镜头DataAsset实例；探索24—35mm、角色50—85mm、技能35—50mm、竞技保持现有FOV；按实际机位和36mm参考传感器重新构图，不机械套用。还需要真实蓝图与后期资源。

P6：接入既有DivineBeastsDemoClient演示编目、可操作参数及镜头焦距/追焦/景深/竞技性能对比；真实创建演示关卡。

P7：在完整修改后一次统一执行Editor/Client/Server目标编译、Automation、Cook剥离、插件展示、多人独立镜头及新手村实际运行；比较同配置硬件下1080p帧时、CPU/GPU、延迟与画质，不凭空声称60FPS。

P8：人工视觉验收和性能留证后，按既定仓库流程复核并合并隔离分支。

## 风险与恢复步骤

1. HWebCodex执行端重新在线后，**先执行git status、git diff与分支差异**，确认其他未提交任务和本批分支状态；不得在未核实本地更改情况下强行切换工作树。
2. 在本地UE5.8编译新增模块，首先检查引擎CinematicCamera具体API/模块名与头文件，再修复C++、UHT及链接错误。
3. 对电影预设和本地玩家摄像机服务执行Editor自动化测试；严禁把测试源码存在当成已执行通过。
4. 使用Monolith/UE编辑器生成和保存真实DataAsset、蓝图及演示地图，不允许用文本代替.uasset/.umap。
5. 确认Client/Server独立构建与Server Cook无纯视觉资源后，形成真实性能/人工验收结论。

## 结论

P0已完成**可访问远程版本的代码级抽样审查**，未包含本地资产/未提交状态全量审查；P1架构决定明确；P2—P3仅初步增量源码；P4—P7未完成；P8仅文档与Git分支增量，无集成验收和正式主分支合并。不得宣称P0—P8全部完成。

## 2026-10-16 本地 UE5.8 实际续作与证据（覆盖先前“仅源码”状态）

执行空间仍为唯一 DivineBeastsWorkspace；本轮本机编辑器确认为 D:\UnrealEngine-5.8.0-release，工作目录为 E:\work\2026\DivineBeastsWorkspace。主分支已包含第一批摄像机C++、蓝图和4份DataAsset，本节不重建工程或插件。期间有多个其他开发任务共用工作空间，本任务只维护独立镜头场景和相应脚本。

真实内容及归属：
- 基础插件 GamePlatformCamera：UGamePlatformCinematicCameraProfile 与 UGamePlatformCinematicCameraSubsystem，归属客户端，仅为可选电影机位，不接管游戏权威。
- 项目内容包 DBAPresentationPack_Core/Cameras/Profiles：4份真实镜头数据资产（28mm探索、65mm角色、45mm技能演出、35mm竞技），均经独立UE编辑器实例成功加载并完成IsValidProfile值校验。
- 开发专用演示地图 /Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview，属于正式Cook排除目录。当前保留4个原生CineCameraActor及1个真实BP_DBA_CinematicCameraRig蓝图Actor实例；后者是Player0默认35mm展示机位，四组24/35/50/85mm原生演员供编辑器分镜对比。
- 真正的 Manny 项目网格、日光、天空、背景及两组立体构图参照物均保留在此地图，未覆盖正式Village/角色创建地图。
- 编辑器作者脚本 Tools/Unreal/Cameras/AuthorSSLMCinematicCameraReview.py 已修复 Unreal Python Rotator 的(roll,pitch,yaw)顺序误用，真实调整摄像机朝向、焦距、光圈、传感器36x20.25mm、到角色目标的追焦距离、开发场景光照，并把默认开发Pawn出生点移出构图以避免白色球体遮挡。全部为演示级配置，不改变比赛公平视野。

已取得的真实证据：
1. 独立编辑器资产回读：Game/Saved/Logs/CinematicCameraBlueprintVerify20261016.log 中 SSLM_CINEMATIC_RELOAD_OK；Saved/Validation/CinematicCamera/IndependentReload.json 确认13个Actor、四组焦距、蓝图继承和蓝图实例本地激活唯一性、相机光轴和焦点距离。
2. 电影镜头DataAsset回读：Saved/Validation/CinematicCamera/ProfilesReload.json 记录全部4份真实配置的有效性；不冒充客户端游戏内加载。
3. 游戏启动：Game/Saved/Logs/CinematicCameraBlueprintGame20261016.log 显示实际 LoadMap、Bringing up level for play，程序正常0退出；Game/Saved/Screenshots/WindowsEditor/HighresScreenshot00031.png 为1280x720真实画面，可见Manny、柱体、阴影和天空，非之前纯黑图。只是独立开发环境基线，色彩和景深未达到最终3A视觉验收标准。
4. 自动化：Game/Saved/Logs/CinematicCameraAutomation20261016.log 记录正式命令行测试 GamePlatform.Camera.Cinematic.Profile.Validation，Found 1、Test Completed Result=Success、Queue Empty 1 tests performed。
5. 客户端摄像机模块：Build.bat DivineBeastsArenaClient Win64 Development -Module=GamePlatformCameraClient 返回 Result: Succeeded；仅证明这一模块构建，不等于整客户端全量功能及完整Cook通过。

未完成与不得宣称项目：
- 尚未完成现有角色预览/新手村/开放世界/竞技赛局的正式电影机位接入、死亡复活/观战状态恢复和控制器无跳变测试；当前电影演示独立于正式主流程，不应覆盖原有相机。
- 尚未在既有DemoCatalog注册所有十类镜头参数交互用例；电影蓝图实例已运行，不代表10项互动面板已实现。
- 尚未进行完整Editor/Client/Server全工程编译、Cook剥离、正式新手村多人场景验证或相同硬件60FPS性能对比；缺证据的一律保持待验收。
- 当前图像仍是简洁的开发白模舞台，真实电影美术品质未通过人工验收；截图为测试证据而非效果宣传图。

演示定位和复测：
- 地图：Game/Content/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview.umap（开发专用，不进入发行包）。
- 蓝图：Game/Content/Development/PluginDemos/Blueprints/BP_DBA_CinematicCameraRig.uasset（默认电影机位）。
- 打开该地图，在编辑器中切换标签为DBA_Cine_24mm、DBA_Cine_35mm、DBA_Cine_50mm、DBA_Cine_85mm四个机位进行焦距比较；35mm默认演示由BP_DBA_CinematicCameraRig蓝图演员接管。
- 参数资产位于 Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/Cameras/Profiles/。
- 一切修改优先通过唯一真实UE5.8编辑器脚本操作，不得手工伪造 .uasset/.umap。

后续重点：完成项目正式摄像机可选接线、界面实际交互、独立场景与多人平台测试，并在得到真实验证之后才可将P0—P8整体宣告完成。
## 2026-10-16 Server完整构建异常与限定回归

为核实ClientOnly模块隔离，先后尝试完整`DivineBeastsArenaServer Win64 Development`构建。引擎自带`ConsoleVariablesEditorRuntime`模块出现MSVC C1001内部编译器错误；再使用`-NoUBA -MaxParallelActions=8`重试，UE自带`Chaos`模块编译时发生访问冲突（错误码-1073741819）。两次均未取得完整Server构建成功证据，中止后避免继续消耗系统资源；没有修改UE引擎源码或伪造编译结果。限定模块`-Module=ConsoleVariablesEditorRuntime -NoUBA -MaxParallelActions=1`成功，`-Module=DivineBeastsArena` Server最小模块检查成功，但都不能替代全目标构建。客户端`DivineBeastsArenaClient -Module=GamePlatformCameraClient`构建真实通过。

编辑器实际运行`GamePlatform.Camera.Cinematic.Profile.Validation`发现1项测试并记录Result=Success；独立编辑器回读地图13个Actor、四组镜头光轴和对焦、蓝图镜头实体及本地玩家唯一激活均通过。正式角色创建、复杂MOBA和完整Cook/性能回归仍属未完成。