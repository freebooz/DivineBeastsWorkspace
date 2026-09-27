# TargetingAndRelationshipBoundary（目标与关系边界）

基础 Target Validation 要求 Source/Target 有效、同一 World、平台 ASC 与 CombatComponent 有效、Owner/Avatar 信息有效、AvatarGeneration 匹配且双方未死亡。

若 Actor 实现 `IGamePlatformCombatant`，额外调用 CanSourceCombat/CanReceiveCombat。ApplyDamage/Healing/Control 必须由 Source 自己的 CombatComponent 发起，Spec.Source 不能冒充其它 Actor。

Self Damage 默认禁止，Self Healing 默认允许，均可通过 CombatSettings 配置；Control 不复用伤害自身策略。

Friendly Fire、MOBA Team、NPC关系、Faction/FiveCamp/Element 不属于平台层，本轮没有恢复这些已取消体系。
