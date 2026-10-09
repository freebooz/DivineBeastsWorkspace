# GamePlatformCombat（游戏平台战斗插件）

**2026-10-09唯一最新战斗设计：** 本插件只保留`UGamePlatformCombatAttributeSet（统一战斗数值属性集）`一种具体数值集。6项字段分别为Health/MaxHealth（生命/上限）、DamageBonus（有符号增伤）、DamageReduction（有符号减伤）、IncomingDamage/IncomingHealing（不复制结算元属性）。已删除Shield/MaxShield（盾永久属性）及独立攻击、防御、穿透、抗性、暴击、韧性类。通过`UGamePlatformShieldGameplayEffect（有限时护盾效果）`及CombatComponent（战斗组件）的服务器有界吸收账本保留护盾玩法，过期/耗尽移除GE，不单独复制盾容量。神兽联盟项目层另持气势2字段，合计2类8字段。见`GamePlatformAbilitySystem/Docs/GAS统一增减伤与护盾效果实施规范_20261009.md`（最新统一结算规范）；模块编译、资源迁移、网络性能与双客户端验收仍须实测。

跨游戏通用 GAS（Gameplay Ability System，玩法能力系统）战斗层。正式仅保留一个 `GamePlatformCombat（战斗运行模块）`，Type 为 Runtime（运行时）；客户端与专用服务器共享类型，但伤害、治疗、控制、死亡和重置只有 Authority（服务器权威）路径可产生最终结果。

当前源码：Combatant（战斗者接口）、CombatComponent（服务器战斗组件）、唯一具体CombatAttributeSet（生命/增减伤/瞬时元属性）、Damage/Healing Execution（伤害/治疗执行）、C++ Damage/Healing/Shield/Stun/Silence GameplayEffect（伤害/治疗/限时盾/眩晕/沉默玩法效果）、有效GE盾容量消耗/溢出/绕盾、死亡幂等、死亡技能取消、重生边界、CombatEvent（战斗事件）、GameplayCue（表现通知）、服务器LineTrace/SphereSweep（线段/球扫命中验证）和AvatarGeneration（角色代次）保护。

`Root（定身）`未实现：GamePlatformCharacter 尚无可复用 MovementBlocked（移动阻止）接口；本轮不通过 Combat→Character 反向依赖强行补齐。Lag Compensation/Server Rewind（延迟补偿/服务器回溯）也未实现，命中验证仅使用服务器当前时刻世界状态。

依赖：GamePlatformCore、GamePlatformAbilitySystem、GameplayAbilities。没有依赖 InputClient、Online、Session、Loading、UI、VFX、MobaCommon、DivineBeasts 或 Go 后端服务。虽然当前仓库已经存在 GamePlatformPresentationCore，本插件仍保持 CombatEvent + GameplayCue 独立出口，没有新增 Presentation 依赖。

2026-10-09新增可选战斗反馈网络投影：`FGamePlatformCombatFeedbackNetEvent`（最小权威事件负载）和`UGamePlatformCombatFeedbackWorldSubsystem`（World范围只读确认事实），通过现有`UGamePlatformCombatComponent::MulticastConfirmedCombatFeedback`（Unreliable服务器单向表现通知）分发事件GUID、角色代次、技能ID、位置和非权威展示数值。该路径不接受客户端任意命中RPC、不更改GAS数据或独立决定击退。对应`Private/Tests/GamePlatformCombatFeedbackNetTests.cpp`仅提供契约源码测试；后续必须以真实Client/Server构建、可见性丢包模拟和双客户端运行证明实际播发行为。

当前状态（2026-09-30）：核心属性扩展已使用 `F:\UnrealEngine-5.8.0-release` 对 `DivineBeastsArenaEditor Win64 Development` 定向编译 `GamePlatformCombat + DivineBeastsCharactersRuntime + DivineBeastsUIClient`，40 个构建动作全部成功。UE Automation 本轮在进入测试队列前被本机 VisionOS SDK 缺少 `MainVersion` 的平台校验阻断；真实 Development 资产、Client/Server 三目标构建、Cook、双客户端 Dedicated Server 联网、延迟模拟仍需后续专项验证。

