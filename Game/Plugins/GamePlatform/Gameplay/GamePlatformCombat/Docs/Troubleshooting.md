# Troubleshooting（故障排查）

Damage 返回 InvalidActorInfo：检查 Source/Target 是否都拥有平台 ASC 和 CombatComponent，并确认 ASC OwnerActor/AvatarActor 已初始化。

Damage 不扣血：检查 EventId 是否已完成、Magnitude 是否超过安全上限、Target AvatarGeneration 是否匹配，以及 GameplayEffect 是否成功进入 IncomingDamage。

Stun/Silence 不阻止技能：检查具体 GameplayAbility 是否带 `Ability.Active`/`Ability.Spell`分类标签；平台控制只阻止被正确分类的 Ability。

晚加入状态不一致：先检查 ASC/AttributeSet/CombatComponent 复制与 Actor relevancy；当前尚无真实专服验证，因此不能把源码设计当成已解决网络问题。
