# CombatCharacterAbilityIntegration（战斗、角色与技能集成）

Characters：
- Arena只使用HeroDefinitionId和可信CharacterId。
- 不写12个Hero class switch。
- Characters不依赖Arena。
- Spawn/Respawn必须通过平台Gameplay/Character统一生命周期。

Combat：
- Combat产生Damage/Death等权威事实。
- Arena统计Score/Kill/Assist等比赛事实。
- Arena不计算伤害；Combat不读取项目Arena类。

Abilities：
- 当前DivineBeastsAbilities Creation Gate为NotRequired。
- 未来Arena可通过Tag/Policy/上层组合限制模式技能。
- Arena不直接GiveAbility，不让Abilities反向依赖Arena。
