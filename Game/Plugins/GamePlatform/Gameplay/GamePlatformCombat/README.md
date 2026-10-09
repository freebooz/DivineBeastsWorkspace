# GamePlatformCombat（游戏平台战斗插件）

**2026-10-09最新属性精简：** 本插件仍保留四个领域属性集，不迁移反射类或新增插件。生命/护盾4项仍公开复制（受Actor网络相关性控制），攻击6项＋防御3项＋Tenacity（控制韧性）1项只向拥有者复制；IncomingDamage/IncomingHealing（结算元属性）2项不复制。因没有真实消费机制，已移除攻击速度与失衡/恢复的4项GAS属性，同时删除失衡重生无效写入。第三层DBAGameplay（项目玩法）保留气势2项且仅向拥有者复制，总计18字段。见 `GamePlatformAbilitySystem/Docs/GAS精简属性集与复制分层实施规范_20261009.md`（三层精简属性实施规范）及 `Tests/Architecture/ValidateCompactGASAttributes.py`（少同步静态门禁）。目前未以静态统计冒充实际网络带宽实测。

跨游戏通用 GAS（Gameplay Ability System，玩法能力系统）战斗层。正式仅保留一个 `GamePlatformCombat（战斗运行模块）`，Type 为 Runtime（运行时）；客户端与专用服务器共享类型，但伤害、治疗、控制、死亡和重置只有 Authority（服务器权威）路径可产生最终结果。

当前已实现：Combatant 接口、CombatComponent、以 `UGamePlatformAttributeSet（平台属性集基类）` 为统一边界的战斗属性体系、Health/MaxHealth、Shield/MaxShield、IncomingDamage/IncomingHealing、`UGamePlatformOffenseAttributeSet（攻击属性集）`、`UGamePlatformDefenseAttributeSet（防御属性集）`、`UGamePlatformControlAttributeSet（控制与韧性属性集）`、Damage/Healing Execution、C++ Damage/Healing GameplayEffect、Stun/Silence 控制 Effect、护盾吸收/溢出、BypassShield、死亡幂等、死亡技能取消、Respawn Reset 边界、CombatEvent、GameplayCue、服务器 LineTrace/SphereSweep 命中验证和 AvatarGeneration 过期保护。

`Root（定身）`未实现：GamePlatformCharacter 尚无可复用 MovementBlocked（移动阻止）接口；本轮不通过 Combat→Character 反向依赖强行补齐。Lag Compensation/Server Rewind（延迟补偿/服务器回溯）也未实现，命中验证仅使用服务器当前时刻世界状态。

依赖：GamePlatformCore、GamePlatformAbilitySystem、GameplayAbilities。没有依赖 InputClient、Online、Session、Loading、UI、VFX、MobaCommon、DivineBeasts 或 Go 后端服务。虽然当前仓库已经存在 GamePlatformPresentationCore，本插件仍保持 CombatEvent + GameplayCue 独立出口，没有新增 Presentation 依赖。

当前状态（2026-09-30）：核心属性扩展已使用 `F:\UnrealEngine-5.8.0-release` 对 `DivineBeastsArenaEditor Win64 Development` 定向编译 `GamePlatformCombat + DivineBeastsCharactersRuntime + DivineBeastsUIClient`，40 个构建动作全部成功。UE Automation 本轮在进入测试队列前被本机 VisionOS SDK 缺少 `MainVersion` 的平台校验阻断；真实 Development 资产、Client/Server 三目标构建、Cook、双客户端 Dedicated Server 联网、延迟模拟仍需后续专项验证。

