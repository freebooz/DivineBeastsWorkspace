# GamePlatformCamera（游戏平台相机插件）

`GamePlatformCamera` 位于 GamePlatform 平台层的 Presentation（表现）域，目标是向客户端、本地玩家和上层游戏组合提供可复用的相机模式、确定性模式栈、镜头混合、目标跟随、碰撞修正和瞬时镜头效果入口。

## 当前真实状态

> 2026-10-09战斗反馈增量：已在现有`GamePlatformCameraClient`新增`UGamePlatformCameraHitFeedbackSubsystem`（仅LocalPlayer镜头冲击）、`GamePlatform.Camera.HitFeedback.UserScale`自动化测试源码。支持预加载CameraShake类、重复事件过滤、最小触发间隔和玩家0倍率关闭；不震动UMG界面、不改变Gameplay权威。完整相机模式栈、镜头衰减曲线资源与真实游戏运行尚未交付；下列旧“只有模块壳”的状态应理解为相机模式系统的历史说明，而非本次HitFeedback功能的现状。


- 当前版本仍为 `0.1.0`，只有 `GamePlatformCameraClient（ClientOnly）` 模块注册壳。
- 当前没有公开相机服务、Camera Mode Definition（相机模式定义）、模式栈、模式／效果句柄、UE 相机适配器、数据租约、自动化测试或 Review Map。
- 当前没有正式模块或目标依赖该插件；不能据此宣称它已进入编辑器、客户端、竞技、观战、死亡或开放世界运行路径。
- `CanContainContent=true` 目前尚无真实内容；方案 A 实施后只允许保存平台中立默认定义和评审资源，项目、世界、英雄及竞技专属相机资产仍归其真实内容所有者。

## 已批准设计方向

采用“方案 A：平台稳定外观＋UE 原生相机适配”：

- `UGamePlatformCameraLocalPlayerSubsystem` 持有本地玩家作用域的服务、请求账本、世界代次和数据租约。
- 私有 `UGamePlatformCameraModifier` 连接当前 `APlayerCameraManager`，复用 UE 的 ViewTarget、CameraModifier、CameraShake 和 SpringArm 能力。
- 持续模式、玩家 Look 输入、瞬时镜头表现使用三个明确分离的契约。
- Definition 由 `GamePlatformData` 统一异步加载和持有，Camera 不建立第二个资产管理器。
- Camera 可注册到 `GamePlatformPresentation` 的中立 `Camera` Provider Channel（提供者通道），但不把持续相机状态强制建模为可选表现事件。
- 平台层不依赖 MobaCommon、DivineBeasts、项目角色、项目地图、UI 或输入实现；项目与 MOBA 层只能向下组合。
- 客户端相机不选择服务器权威目标，不处理命中、伤害、权威瞄准或网络准入。

## 设计与实施状态

完整的职责、依赖、公开契约、状态机、生命周期、错误语义、性能边界、测试矩阵和交付门禁见 [ImplementationSpecification.md](Docs/ImplementationSpecification.md)。

该规格已经固化方案 A，但运行代码尚未实施。下一阶段必须先由人工复核书面规格，再形成逐文件、测试驱动的实施计划；在真实 UE 编译、Automation、Review、Client Cook 和 Server 产物审计完成前，不得把规划项标记为已交付。


## 2026-10-15电影镜头源码增量（待实际编译）

本轮新增 `UGamePlatformCinematicCameraProfile`（电影镜头数据资产）和 `UGamePlatformCinematicCameraSubsystem`（本地玩家电影机位），以及数据校验Automation源码。这是对现有ClientOnly模块的增量，不是另建摄像机插件，也不意味着原规格中的模式账本、异步Data租约、输入迁移、完整CameraModifier已经实现。

进入临时展示机位：在当前LocalPlayer上获取 `UGamePlatformCinematicCameraSubsystem`，调用 `BeginCinematicView(CameraActor, Profile)`；只允许当前本地世界已存在的Actor，返回false不修改视角。业务调用使用 `EndCinematicViewFor(CameraActor)` 按目标释放；目标与当前租约Owner不符时返回false且不修改视角或租约。仅管理唯一电影机位的开发演示可继续调用原无参数 `EndCinematicView()`。过渡使用引擎`SetViewTargetWithBlend`。电影Actor的传感器宽度、焦距及非竞技光圈/手动对焦由Profile设置；配置仅供客户端表现，不改变权威行为。

**截至2026-10-15仍未完成**：在角色展示工作室及正式场景内创建/接入原生CineCamera、数据实例、序列和Demo；与已有输入/预览相机及MOBA视角进行完整整合；模式优先级和可抢占恢复；真实资产创建、Editor/Client/Server编译、Automation运行、Cook剥离及性能测量。该增量当时尚未在UE5.8中编译。后续正式角色预览源码增量与实际验证见下文。

## 2026-10-16 真实资产与验收补充

以前旧段落中的“未创建任何资源/未编译”只是2026-09-29至10-15早期计划基线，不再代表本次电影镜头增量状态。当前确有Cinema Profile C++反射类、本地玩家临时机位子系统、项目层四份镜头DataAsset、蓝图演员BP_DBA_CinematicCameraRig和真实开发ReviewMap。独立UE5.8编辑器回读已经验证镜头24/35/50/85mm、角色追焦方向和展示蓝图唯一激活；实际游戏关卡进入运行阶段并已截图。原有完整Camera Mode账本和Data租约仍未实施，角色创建、竞技、观战等正式业务未全部接线。详细证据与未完成门禁见 Docs/Implementation/SSLMCinematicCameraExecution_20261015.md。

## 2026-10-16 可选镜头重定向

同一本地玩家拥有有效电影机位时，BeginCinematicView再次申请同一Actor保持幂等；更换为另一已预置Actor时，由UE原生SetViewTargetWithBlend连续混合，并保留最初退出恢复目标。其它系统、世界或Controller抢占后应拒绝修改。此能力完全属于ClientOnly，不能代替完整相机模式栈或正式比赛策略；开发对比镜头已复用，生产场景仍待验收。

## 2026-10-18 正式角色预览增量

正式角色预览退出使用 `EndCinematicViewFor(PreviewStage)` 精确释放对应舞台的租约。若目标不匹配，平台子系统保持当前机位租约及恢复目标不变，避免角色预览结束时释放登录页或其他界面申请的电影机位。开发演示保留原有无参数 `EndCinematicView()` 接口。

UE5.8.0完整Editor目标构建通过，Editor构建证据为Eligible；8项定向原生自动化测试全部通过，其中 `GamePlatform.Camera.Cinematic.LeaseOwnership` 覆盖错配目标保护。具体命令与报告路径见 `Docs/Implementation/Cameras/SSLM_FormalCharacterPreviewCameraIntegration_20261018.md`。自动化未完成正式场景人工画面、鼠标操作、焦点/输入交接、Cook、服务器隔离及目标设备性能验收。
