# GamePlatformAnimation（游戏平台动画插件）

当前状态：**一期关键能力未完成**。插件已保留 `GamePlatformAnimation（Runtime）` 与 `GamePlatformAnimationClient（ClientOnly）` 模块身份，但当前源码仍是模块壳，不能作为动画框架已交付的证明。

目标边界：Runtime 只承载双端必须共享的 Animation Semantic、权威 Montage／RootMotion 时序和动作身份；ClientOnly 承载 Anim Layer、Linked Anim Graph、Locomotion、Motion Warping、Foot IK、Hit Reaction 等纯表现能力。不得建立第二套 GAS 或让动画表现决定权威战斗结果。

详细实施方案见 [ImplementationSpecification.md](Docs/ImplementationSpecification.md)。后续必须补自动测试、真实 UE Review 场景、Client/Server Cook 和网络时序验证后再提升验收级别。
