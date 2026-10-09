# GamePlatformAnimation（游戏平台动画插件）

当前状态：**一期关键能力未完成**。插件已保留 `GamePlatformAnimation（Runtime）` 与 `GamePlatformAnimationClient（ClientOnly）` 模块身份，但当前源码仍是模块壳，不能作为动画框架已交付的证明。

目标边界：Runtime 只承载双端必须共享的 Animation Semantic、权威 Montage／RootMotion 时序和动作身份；ClientOnly 承载 Anim Layer、Linked Anim Graph、Locomotion、Motion Warping、Foot IK、Hit Reaction 等纯表现能力。不得建立第二套 GAS 或让动画表现决定权威战斗结果。

详细实施方案见 [ImplementationSpecification.md](Docs/ImplementationSpecification.md)。后续必须补自动测试、真实 UE Review 场景、Client/Server Cook 和网络时序验证后再提升验收级别。

2026-10-09新增 `GamePlatformAnimationClient/Feedback/GamePlatformLocalHitstopSubsystem`（客户端局部视觉顿帧）：按60Hz参考帧换算、命中GUID有界去重、单调实时时钟按需驱动、较长剩余时间覆盖及连续10帧窗口封顶，并在世界退出恢复。当前只暂停客户端骨骼Mesh动画的`bPauseAnims`，**不能当作已经冻结RootMotion、真实击退积分或Gameplay硬直**；全局动画框架仍需另行实现。三层执行计划见 `Docs/Implementation/CombatFeedbackExecutionPlan_20261009.md`。

新增 `GamePlatformHitFlashWorldSubsystem`（客户端受击网格短时闪白），通过原生`UMeshComponent::SetOverlayMaterial`作用于被命中模型，不进行全屏闪白，也不每次创建动态材质实例；参考上限2帧、最多64活动网格、恢复先前Overlay并在World退出清理。用户需由真实预加载Profile提供材质资产，空资产时自动跳过；尚不等于实际完成闪白材质和战斗镜头演示。

2026-10-09安全边界补充：`ApplyVisualHitstop`仅当至少一项Mesh实际接纳冻结后返回成功；已有其他系统拥有的`bPauseAnims`状态及`ACharacter::IsPlayingRootMotion`角色一律跳过。该保护防止未完成的视觉根运动分离影响客户端预测，完整RootMotion同步仍需专用服务器双端运行验收。
