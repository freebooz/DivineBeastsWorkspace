# CombatModel（战斗模型）

Combatant（战斗参与者）由通用 Actor + `UGamePlatformAbilitySystemComponent` + `UGamePlatformCombatComponent`组成；Actor 可选择实现 `IGamePlatformCombatant`提供更细的 CanSourceCombat/CanReceiveCombat 策略。

每次结算使用 `FGamePlatformCombatSpec`：EventId、Source、Target、Magnitude、CombatTags、EffectContext、服务器 HitContext、AbilityId 和 Source/Target AvatarGeneration。

`UGamePlatformCombatComponent`维护最多 256 个已完成 EventId，重复 EventId 返回 Cancelled 或被 Attribute 回调忽略，避免同一事件重复扣血/重复死亡。

Relationship（关系）、Team（队伍）、Friendly Fire（友军伤害）不是平台 Combat 的硬编码职责，后续由上层策略扩展 Target Validation。
