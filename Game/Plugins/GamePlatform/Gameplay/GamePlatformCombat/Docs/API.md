# API（公开接口）

主要公开类型：`IGamePlatformCombatant`、`IGamePlatformCombatStateProvider`、`UGamePlatformCombatComponent`、`UGamePlatformCombatAttributeSet`、`FGamePlatformCombatSpec/Result/Event`、Damage/Healing Execution、Stun/Silence GameplayEffect 和 `FGamePlatformCombatHitValidator`。

`ApplyDamage/ApplyHealing/ApplyControl/RemoveControl/ResetForNewAvatar`标记为 `BlueprintAuthorityOnly`，不是 Server RPC；客户端调用不会自动上送服务器。

CombatSpec 不接受 EffectClass 路径、数据库对象、UI/VFX 对象或 MOBA Team 结构。Damage/Healing 数值仍在服务器侧进行有限数值和安全上限验证。

状态查询接口只暴露 Health/Shield/Dead/AvatarGeneration，不提供客户端修改权威状态的接口。
