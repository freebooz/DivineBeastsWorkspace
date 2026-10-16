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
