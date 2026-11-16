# SSLM 正式角色展示电影镜头接入记录（2026-10-18）

## 基线核验

检查的主分支基线提交为 `f8736343554a103167d491169f74d4e720b278e6`，时间为2026-10-18 12:42（北京时间）。

- 独立电影镜头演示已有UE5.8构建、运行、切镜和UMG截图记录；这些证据只代表开发演示。
- 正式角色选择与创建仍共用 `UDivineBeastsCharacterPreviewSubsystem`（角色预览子系统）和 `AGamePlatformCharacterPreviewStage`（平台角色预览舞台），并流送 `/DBAFrontEndPack/Maps/L_DBA_CharacterStudio`（角色展示关卡）。
- 正式流程已有0.45秒进场混合设置，但退出时开始视角恢复后立即隐藏并卸载流送关卡，过渡源可能提前消失。
- 正式舞台使用普通 `UCameraComponent`（相机组件）；原 `UGamePlatformCinematicCameraSubsystem`（平台电影镜头子系统）只给电影相机组件应用焦段，现有角色舞台没有正式的焦段控制入口。
- 本记录初稿时没有可用的UE5.8项目运行环境，因此正式角色关卡当时没有新的运行截图或实机输入证据；本次后续验证结果见文末。

## 实际接入

没有创建新插件、镜头管理器、审核地图、镜头资产或第二套角色预览系统。

- 正式预览子系统通过已有平台镜头子系统取得本地视角租约。租约仍检查玩家控制器、世界、网络模式及当前视角所有权；失去所有权时不覆盖登录、游戏或更高优先级镜头。
- 平台相机服务现在可将36毫米传感器宽度与焦距转换为普通相机的等效水平视场角；真正的电影相机继续使用原生光学参数。同一镜头目标收到新配置时更新镜头但不重复切镜。
- 角色预览使用临时、仅本地的 `CharacterPreview` 配置：默认35毫米，焦段限制24–85毫米，混合时间沿用舞台的 `CameraBlendSeconds`（镜头混合秒数），关闭浅景深追焦，保持角色和创建页面文字清楚。配置不会作为内容资产保存或进入服务器。
- 普通鼠标拖动继续通过现有ViewModel（视图模型）命令旋转角色。鼠标指针位于现有预览区域时，滚轮每格将视距调整30厘米；按住Shift滚轮每格调整焦距5毫米。其他表单、列表及输入焦点保持原路径。
- 选择十二生肖只替换角色外观软资源；镜头位置和焦段保持稳定，不重建相机或打断异步加载。
- 正常退出通过平台镜头服务的 `EndCinematicViewFor(PreviewStage)`（按预览舞台目标释放机位租约）恢复原ViewTarget（视角目标），避免误结束登录页或其他界面的机位。仅当恢复混合确实启动时，保留舞台、角色外观及流送关卡至混合时间结束后50毫秒，再还原原始视场角并卸载；退出混合期间再次进入会取消旧卸载任务。
- 竞技模式配置明确跳过焦段、光圈和手动对焦设置；角色预览功能只位于客户端表现模块。Dedicated Server（专用服务器）不新增相机模块依赖或展示资源。

## 本次修改文件

- `GamePlatformCinematicCameraProfile.h/.cpp`（平台镜头配置类）：增加焦距到水平视场角换算。
- `GamePlatformCinematicCameraSubsystem.h/.cpp`（平台镜头子系统）：普通相机焦段应用、竞技光学隔离、世界清理，以及按目标释放租约。
- `GamePlatformCinematicCameraProfileTests.cpp`（平台镜头自动化测试）：覆盖24/35/50/85毫米视场角、普通相机应用、竞技隔离及错误目标不得释放其他机位租约。
- `GamePlatformCharacterPreviewStage.h`（平台预览舞台）：公开当前安全视距供滚轮命令读取。
- `DivineBeastsCharacterPreviewSubsystem.h/.cpp`（项目角色预览子系统）：接入平台相机租约、焦段配置、视距调整、原视场恢复及延迟卸载。
- `DivineBeastsPresentationClient.Build.cs`（项目客户端表现模块依赖）：只为ClientOnly（仅客户端）模块添加 `GamePlatformCameraClient`（游戏平台相机客户端模块）。
- `DivineBeastsUIViewModel.h/.cpp`（项目界面视图模型）和角色选择/创建页面：在现有预览区域接入滚轮距离与焦段操作。
- `DivineBeastsPreviewDragState.h` 及 `DivineBeastsPreviewDragTests.cpp`（预览手势状态及测试）：新增滚轮增量、方向和限幅回归。

## 人工审核入口

使用正常客户端登录后进入 `UI.Screen.CharacterSelect`（角色选择页面）或 `UI.Screen.CharacterCreate`（角色创建页面）。两页共用角色展示关卡和预览舞台。

1. 在三维角色预览区域拖动鼠标，确认角色可旋转。
2. 在预览区域滚动滚轮，确认相机远近变化；按住Shift滚轮，确认焦段变化。
3. 切换多个生肖，确认外观加载仍使用既有异步资源租约且相机不会重置或被登录界面抢回。
4. 离开展示页，确认镜头平滑回到原视角并恢复舞台初始视场；随后继续登录流程。
5. 进入MainArena（主竞技场），确认其正式竞技视角、视场和清晰度不受角色展示配置影响。

## 验证状态

- 已核对主分支最新源码、相机插件实现、正式预览舞台、选择/创建页面输入链及历史实施记录。
- 本地公式校核结果：24/35/50/85毫米对应约73.7398°/54.4322°/39.5978°/23.9132°水平视场角。
- 初稿新增了焦段映射、竞技隔离与预览滚轮自动化测试；其实际UE5.8构建及运行结果见“UE5.8后续验证”。
- 尚无正式角色选择/创建的新版实机截图、鼠标轮操作日志、退出恢复录像、Cook结果或真实帧率数据。正式画面、交互、输入交接及性能仍需在UE5.8编辑器和目标设备完成最终验收。

## UE5.8 后续验证（2026-10-18）

初次接入基线为主分支 `4dde0b951ab3e5459f0f25f7f8ff483ae3a53ca0`。主分支随后推进至 `ccb72e8dcaa18fa8e9def8ca52b29f8217f94544`；分支已重放于该最新提交之上，并在此基线重新构建和运行自动化。当前相对主分支无落后提交，`git diff --check origin/main...HEAD` 通过。原主工作区的未提交文件和运行任务未改动。

- UE5.8.0 Editor 项目目标构建通过，UBT 259/259 actions 成功，进程退出码0；构建证据标记 `Status=Passed`、`EditorBuildEvidenceStatus=Eligible`。使用 `Build/Game/BuildFoundation.ps1 -EngineRoot D:\UnrealEngine-5.8.0-release -Editor -UsePrecompiled -AdditionalPlugins DBAServer -NoUBA -MaxParallelActions 8`。证据：`Saved/Validation/FoundationM0/f16df79c-d485-41f7-bc79-fbe4cab3c043/Build/result.json` 及 `Editor/UBT.log`。该配置复用现有UE引擎二进制，不代表引擎源码重建。
- UE5.8 `UnrealEditor-Cmd` 前端地图自动化结果为 `Passed=true`、`Count=7`、`Failed=0`。七项均为 `Success`：`GamePlatform.Camera.Cinematic.Profile.Validation`、`GamePlatform.Camera.Cinematic.FocalLengthMapping`、`GamePlatform.Camera.Cinematic.GenericCameraOpticsAndArenaIsolation`、`DivineBeasts.UI.Characters.PreviewCameraWheel`、`DivineBeasts.UI.Characters.PreviewDragDirection`、`DivineBeasts.UI.Characters.PreviewDragFiniteClamp`、`DivineBeasts.UI.Characters.PreviewDragLifetime`。原生报告：`Saved/Validation/CastingMonster20261013/Native-971f7403-3552-4db2-badf-34d6f8140936/Report/index.json`；汇总：同目录 `Result.json`。
- 后续租约所有权修正通过UE5.8.0完整Editor目标构建，退出码0，`EditorBuildEvidenceStatus=Eligible`；本次增量构建执行6个UBT动作。采用 `Build/Game/BuildFoundation.ps1 -Editor -UsePrecompiled -AdditionalPlugins DBAServer -MaxParallelActions 2`。证据：`Saved/Validation/FoundationM0/236f9818-b165-4bb6-ad60-35791a1dcdac/Build/result.json` 与 `Build/Editor/UBT.log`。复用现有引擎二进制，不代表引擎源码重建。
- 同一工作树重新运行8项UE5.8自动化测试，`Passed=true`、`Count=8`、`Failed=0`。除原七项外新增 `GamePlatform.Camera.Cinematic.LeaseOwnership`，验证预览传入不同舞台目标时不会清除其他电影机位租约。报告：`Saved/Validation/CastingMonster20261013/Native-6f28bad1-691e-4f9a-87fd-337fbc38f53a/Report/index.json`；汇总：同目录 `Result.json`。
- 本次报告仍明确为 `NetworkAcceptance=false`、`ManualVisualAccepted=false`。测试载入前端地图但没有交互操作正式角色选择/创建页面；正式场景截图、输入交接、生肖实机切换、镜头抢占/恢复录像、Cook、专用服务器隔离运行和目标设备性能验收仍未完成。
- 重放到主分支 `ccb72e8dcaa18fa8e9def8ca52b29f8217f94544` 后，再次通过UE5.8.0完整Editor目标构建，`UBT 24/24 actions`、退出码0、`EditorBuildEvidenceStatus=Eligible`。命令仍复用UE引擎预编译二进制并额外装配DBAServer。证据：`Saved/Validation/FoundationM0/9804e9b8-4dd2-4c86-9b14-8f54c401ac4c/Build/result.json` 与 `Build/Editor/UBT.log`。
- 在该最新主分支基线上再次精确运行8项自动化测试，`Passed=true`、`Count=8`、`Failed=0`，租约归属、焦段、竞技隔离、滚轮和拖拽用例均为 `Success`。原生报告：`Saved/Validation/CastingMonster20261013/Native-78bd6100-1d1b-47ca-9521-4f9693117634/Report/index.json`；汇总：同目录 `Result.json`。仍为 `NetworkAcceptance=false`、`ManualVisualAccepted=false`。
- 自动化覆盖焦段映射、普通相机光学应用、竞技参数隔离，以及预览滚轮和拖拽状态；结果明确保留 `NetworkAcceptance=false`、`ManualVisualAccepted=false`。测试进程载入前端地图，但没有通过交互式编辑器操作正式角色选择/创建页面。
- 因当前没有UE编辑器UI自动化通道，本次没有改动蓝图或正式关卡资源，也没有新的正式场景截图、真人输入、生肖切换、镜头抢占/恢复录像、Cook、专用服务器隔离运行或目标设备性能记录。上述项目仍需按“人工审核入口”完成视觉与运行验收。
