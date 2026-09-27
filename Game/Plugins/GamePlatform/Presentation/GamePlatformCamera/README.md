# GamePlatformCamera（游戏平台相机插件）

当前状态：**一期关键客户端表现能力未完成**。现有 `GamePlatformCameraClient（ClientOnly）` 仅为模块入口，没有正式 CameraMode、ModeStack、Definition、Target／Blend／Collision 或 Review。

目标是 LocalPlayer 作用域、Definition 驱动的通用相机框架，复用 UE PlayerCameraManager／CameraModifier／CameraShake，不实现服务器权威瞄准或项目专属英雄规则。

详细实施方案见 [ImplementationSpecification.md](Docs/ImplementationSpecification.md)。真实客户端资产、Review Map、手柄／分辨率／碰撞／高速移动和性能验证未执行。
