# GamePlatformCombat（游戏平台战斗插件）

**2026-10-09最新决定：不要韧性设计。** 本插件仅保留生命/护盾、攻击、防御3组具体战斗属性集，全部继承中立 `UGamePlatformAttributeSet（平台属性集基类）`。已移除 `UGamePlatformControlAttributeSet（历史控制属性集）` 及 Tenacity（控制韧性）字段、控制时长抗性减免公式和动态注册逻辑。服务器仍校验并应用Stun/Silence（眩晕/沉默）玩法效果，持续时间按技能可信请求直接设置。项目另有气势属性集，合计4组17个字段：4个公开生命/护盾、11个拥有者私有、2个不复制元属性。参见 `GamePlatformAbilitySystem/Docs/GAS无韧性精简属性实施规范_20261009.md`（最新无韧性属性规范）和 `Tests/Architecture/ValidateCompactGASAttributes.py`（精简属性门禁）。UE编译和双客户端网络测试仍需以本次真实结果验收。

跨游戏通用 GAS（Gameplay Ability System，玩法能力系统）战斗层。正式仅保留一个 `GamePlatformCombat（战斗运行模块）`，Type 为 Runtime（运行时）；客户端与专用服务器共享类型，但伤害、治疗、控制、死亡和重置只有 Authority（服务器权威）路径可产生最终结果。

当前已实现：Combatant 接口、CombatComponent、以 `UGamePlatformAttributeSet（平台属性集基类）` 为统一边界的战斗属性体系、Health/MaxHealth、Shield/MaxShield、IncomingDamage/IncomingHealing、`UGamePlatformOffenseAttributeSet（攻击属性集）`、`UGamePlatformDefenseAttributeSet（防御属性集）`、Damage/Healing Execution、C++ Damage/Healing GameplayEffect、Stun/Silence 控制 Effect、护盾吸收/溢出、BypassShield、死亡幂等、死亡技能取消、Respawn Reset 边界、CombatEvent、GameplayCue、服务器 LineTrace/SphereSweep 命中验证和 AvatarGeneration 过期保护。

`Root（定身）`未实现：GamePlatformCharacter 尚无可复用 MovementBlocked（移动阻止）接口；本轮不通过 Combat→Character 反向依赖强行补齐。Lag Compensation/Server Rewind（延迟补偿/服务器回溯）也未实现，命中验证仅使用服务器当前时刻世界状态。

依赖：GamePlatformCore、GamePlatformAbilitySystem、GameplayAbilities。没有依赖 InputClient、Online、Session、Loading、UI、VFX、MobaCommon、DivineBeasts 或 Go 后端服务。虽然当前仓库已经存在 GamePlatformPresentationCore，本插件仍保持 CombatEvent + GameplayCue 独立出口，没有新增 Presentation 依赖。

2026-10-09新增可选战斗反馈网络投影：`FGamePlatformCombatFeedbackNetEvent`（最小权威事件负载）和`UGamePlatformCombatFeedbackWorldSubsystem`（World范围只读确认事实），通过现有`UGamePlatformCombatComponent::MulticastConfirmedCombatFeedback`（Unreliable服务器单向表现通知）分发事件GUID、角色代次、技能ID、位置和非权威展示数值。该路径不接受客户端任意命中RPC、不更改GAS数据或独立决定击退。对应`Private/Tests/GamePlatformCombatFeedbackNetTests.cpp`仅提供契约源码测试；后续必须以真实Client/Server构建、可见性丢包模拟和双客户端运行证明实际播发行为。

当前状态（2026-09-30）：核心属性扩展已使用 `F:\UnrealEngine-5.8.0-release` 对 `DivineBeastsArenaEditor Win64 Development` 定向编译 `GamePlatformCombat + DivineBeastsCharactersRuntime + DivineBeastsUIClient`，40 个构建动作全部成功。UE Automation 本轮在进入测试队列前被本机 VisionOS SDK 缺少 `MainVersion` 的平台校验阻断；真实 Development 资产、Client/Server 三目标构建、Cook、双客户端 Dedicated Server 联网、延迟模拟仍需后续专项验证。

