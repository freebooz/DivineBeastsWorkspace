# GamePlatformCamera（游戏平台相机插件）

`GamePlatformCamera` 位于 GamePlatform 平台层的 Presentation（表现）域，目标是向客户端、本地玩家和上层游戏组合提供可复用的相机模式、确定性模式栈、镜头混合、目标跟随、碰撞修正和瞬时镜头效果入口。

## 当前真实状态

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
