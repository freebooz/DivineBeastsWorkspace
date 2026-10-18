# SSLM 正式角色展示电影镜头接入记录（2026-10-18）

## 基线核验

检查的主分支基线提交为 `f8736343554a103167d491169f74d4e720b278e6`，时间为2026-10-18 12:42（北京时间）。

- 独立电影镜头演示已有UE5.8构建、运行、切镜和UMG截图记录；这些证据只代表开发演示。
- 正式角色选择与创建仍共用 `UDivineBeastsCharacterPreviewSubsystem`（角色预览子系统）和 `AGamePlatformCharacterPreviewStage`（平台角色预览舞台），并流送 `/DBAFrontEndPack/Maps/L_DBA_CharacterStudio`（角色展示关卡）。
- 正式流程已有0.45秒进场混合设置，但退出时开始视角恢复后立即隐藏并卸载流送关卡，过渡源可能提前消失。
- 正式舞台使用普通 `UCameraComponent`（相机组件）；原 `UGamePlatformCinematicCameraSubsystem`（平台电影镜头子系统）只给电影相机组件应用焦段，现有角色舞台没有正式的焦段控制入口。
- 当前会话没有本地项目检出、UE5.8编辑器或命令行构建工具，因此正式角色关卡没有新的运行截图或实机输入证据。

## 实际接入

没有创建新插件、镜头管理器、审核地图、镜头资产或第二套角色预览系统。

- 正式预览子系统通过已有平台镜头子系统取得本地视角租约。租约仍检查玩家控制器、世界、网络模式及当前视角所有权；失去所有权时不覆盖登录、游戏或更高优先级镜头。
- 平台相机服务现在可将36毫米传感器宽度与焦距转换为普通相机的等效水平视场角；真正的电影相机继续使用原生光学参数。同一镜头目标收到新配置时更新镜头但不重复切镜。
- 角色预览使用临时、仅本地的 `CharacterPreview` 配置：默认35毫米，焦段限制24–85毫米，混合时间沿用舞台的 `CameraBlendSeconds`（镜头混合秒数），关闭浅景深追焦，保持角色和创建页面文字清楚。配置不会作为内容资产保存或进入服务器。
- 普通鼠标拖动继续通过现有ViewModel（视图模型）命令旋转角色。鼠标指针位于现有预览区域时，滚轮每格将视距调整30厘米；按住Shift滚轮每格调整焦距5毫米。其他表单、列表及输入焦点保持原路径。
- 选择十二生肖只替换角色外观软资源；镜头位置和焦段保持稳定，不重建相机或打断异步加载。
- 正常退出由平台镜头服务按所有权恢复原ViewTarget（视角目标）。仅当恢复混合确实启动时，保留舞台、角色外观及流送关卡至混合时间结束后50毫秒，再还原原始视场角并卸载；退出混合期间再次进入会取消旧卸载任务。
- 竞技模式配置明确跳过焦段、光圈和手动对焦设置；角色预览功能只位于客户端表现模块。Dedicated Server（专用服务器）不新增相机模块依赖或展示资源。

## 本次修改文件

- `GamePlatformCinematicCameraProfile.h/.cpp`（平台镜头配置类）：增加焦距到水平视场角换算。
- `GamePlatformCinematicCameraSubsystem.h/.cpp`（平台镜头子系统）：普通相机焦段应用、竞技光学隔离及世界清理时释放失效租约。
- `GamePlatformCinematicCameraProfileTests.cpp`（平台镜头自动化测试）：覆盖24/35/50/85毫米视场角、普通相机应用及竞技隔离。
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
- 已增加UE自动化测试：`GamePlatform.Camera.Cinematic.FocalLengthMapping`、`GamePlatform.Camera.Cinematic.GenericCameraOpticsAndArenaIsolation` 和 `DivineBeasts.UI.Characters.PreviewCameraWheel`。当前会话无法启动UE编辑器，测试未执行，不能据此宣称C++编译通过。
- 尚无正式角色选择/创建的新版实机截图、鼠标轮操作日志、退出恢复录像、Cook结果或真实帧率数据。正式画面、交互、输入交接及性能仍需在UE5.8编辑器和目标设备完成最终验收。
