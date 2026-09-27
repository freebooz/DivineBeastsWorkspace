# GamePlatformCombat（游戏平台战斗插件）

跨游戏通用 GAS（Gameplay Ability System，玩法能力系统）战斗层。正式仅保留一个 `GamePlatformCombat（战斗运行模块）`，Type 为 Runtime（运行时）；客户端与专用服务器共享类型，但伤害、治疗、控制、死亡和重置只有 Authority（服务器权威）路径可产生最终结果。

本轮已实现：Combatant 接口、CombatComponent、CombatAttributeSet、Health/MaxHealth、Shield/MaxShield、IncomingDamage/IncomingHealing、Damage/Healing Execution、C++ Damage/Healing GameplayEffect、Stun/Silence 控制 Effect、护盾吸收/溢出、BypassShield、死亡幂等、死亡技能取消、Respawn Reset 边界、CombatEvent、GameplayCue、服务器 LineTrace/SphereSweep 命中验证和 AvatarGeneration 过期保护。

`Root（定身）`未实现：GamePlatformCharacter 尚无可复用 MovementBlocked（移动阻止）接口；本轮不通过 Combat→Character 反向依赖强行补齐。Lag Compensation/Server Rewind（延迟补偿/服务器回溯）也未实现，命中验证仅使用服务器当前时刻世界状态。

依赖：GamePlatformCore、GamePlatformAbilitySystem、GameplayAbilities。没有依赖 InputClient、Online、Session、Loading、UI、VFX、MobaCommon、DivineBeasts 或 Go 后端服务。虽然当前仓库已经存在 GamePlatformPresentationCore，本插件仍保持 CombatEvent + GameplayCue 独立出口，没有新增 Presentation 依赖。

当前状态：源码和静态边界门禁通过；UE5.8 编译、UE Automation 执行、真实 Development 资产、三目标构建、Client/Server Cook、双客户端 Dedicated Server 联网、延迟模拟与人工审查均未执行，因为 Runner 没有锁定/可用 UE5.8 工具链。

