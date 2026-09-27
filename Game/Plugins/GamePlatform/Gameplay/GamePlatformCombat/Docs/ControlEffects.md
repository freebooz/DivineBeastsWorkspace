# ControlEffects（控制效果）

本轮真实支持 Stun（眩晕）和 Silence（沉默）。两者使用 Duration GameplayEffect、AggregateByTarget 单层堆叠、成功重施刷新持续时间。

Stun 授予 `Combat.Control.Stun`并阻止 `Ability.Active`；Silence 授予 `Combat.Control.Silence`并阻止 `Ability.Spell`。控制时长必须大于 0 且不超过 `MaxControlDuration`。

`RemoveControl`支持服务器显式移除；Effect 到期/死亡清理/显式移除导致标签计数归零时，CombatComponent 发布 ControlRemoved CombatEvent。

Root（定身）本轮未实现，因为 GamePlatformCharacter 仍是骨架且没有经过验证的 MovementBlocked 适配接口；Combat 没有为此反向依赖 Character。
