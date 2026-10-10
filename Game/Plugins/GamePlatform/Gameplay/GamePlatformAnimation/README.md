# GamePlatformAnimation（游戏平台动画插件）

当前状态：**一期完整动画框架未完成**。插件保留 `GamePlatformAnimation（Runtime）` 与 `GamePlatformAnimationClient（ClientOnly）` 模块身份；前者仍是基础装配，后者已具有实际的局部视觉顿帧、目标Overlay闪白与自动化测试源码，但没有完成完整角色动画、RootMotion/同步、资产和联机验收，不能称为整个动画框架已经交付。

目标边界：Runtime 只承载双端必须共享的 Animation Semantic、权威 Montage／RootMotion 时序和动作身份；ClientOnly 承载 Anim Layer、Linked Anim Graph、Locomotion、Motion Warping、Foot IK、Hit Reaction 等纯表现能力。不得建立第二套 GAS 或让动画表现决定权威战斗结果。

详细实施方案见 [ImplementationSpecification.md](Docs/ImplementationSpecification.md)。后续必须补自动测试、真实 UE Review 场景、Client/Server Cook 和网络时序验证后再提升验收级别。

2026-10-09新增 `GamePlatformAnimationClient/Feedback/GamePlatformLocalHitstopSubsystem`（客户端局部视觉顿帧）：按60Hz参考帧换算、命中GUID有界去重、单调实时时钟按需驱动、较长剩余时间覆盖及连续10帧窗口封顶，并在世界退出恢复。当前只暂停客户端骨骼Mesh动画的`bPauseAnims`，**不能当作已经冻结RootMotion、真实击退积分或Gameplay硬直**；全局动画框架仍需另行实现。三层执行计划见 `Docs/Implementation/CombatFeedbackExecutionPlan_20261009.md`。

新增 `GamePlatformHitFlashWorldSubsystem`（客户端受击网格短时闪白），通过原生`UMeshComponent::SetOverlayMaterial`作用于被命中模型，不进行全屏闪白，也不每次创建动态材质实例；参考上限2帧、最多64活动网格、恢复先前Overlay并在World退出清理。用户需由真实预加载Profile提供材质资产，空资产时自动跳过；尚不等于实际完成闪白材质和战斗镜头演示。

2026-10-09安全边界补充：`ApplyVisualHitstop`仅当至少一项Mesh实际接纳冻结后返回成功；已有其他系统拥有的`bPauseAnims`状态及`ACharacter::IsPlayingRootMotion`角色一律跳过。该保护防止未完成的视觉根运动分离影响客户端预测，完整RootMotion同步仍需专用服务器双端运行验收。

2026-10-09 调试改进：新增仅客户端本地控制台变量 gp.Combat.HitstopOverrideFrames（参考帧覆盖）。-1读取数据资产值，0关闭视觉顿帧，3/6可在同一技能下比较手感，上限10帧，不影响服务器伤害、全局时间或其它表现层。新增 GamePlatform.Animation.Hitstop.Tuning UE自动化测试源码，覆盖0/3/6与非法输入边界；引擎实际测试执行仍待验证。

2026-10-10 UE5.8新编译实证：Editor目标`-Module=GamePlatformAnimation`实际完成25/25动作，包含Core引擎依赖与DLL链接，UBT返回成功0；`GamePlatformAnimationClient`中的`GamePlatformLocalHitstopSubsystem.cpp`、`GamePlatformHitFlashWorldSubsystem.cpp`与顿帧参数测试源码分别`-SingleFile`实际MSVC编译成功，退出码均0。注意：模块定向编译后插件尚未生成可供完整Editor装载的`UnrealEditor.modules`，ClientOnly模块DLL仍待真实构建；需完成全目标编译、Automation、Cook和手工打击感审查。具体证据见`Docs/Implementation/CombatFeedbackBuildRecovery_20261010.md`。
