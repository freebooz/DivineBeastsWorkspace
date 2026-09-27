# DeathAndRecovery（死亡与恢复）

Damage 结算满足 PreviousHealth>0、RemainingHealth<=0 且当前非 Dead 时进入死亡。`bDead`只在 Authority 设置，并添加可复制的 `Combat.State.Dead` loose tag。

死亡时取消带 `Ability.Active`分类的技能，并移除 Stun/Silence 活动 Effect；同一 EventId 通过 CompletedEventIds 阻止重复结算和重复死亡。

Combat 只发布 Death CombatEvent 和 GameplayCue，不调用 RestartPlayer、不决定 5 秒复活、不计击杀/比赛分数。

`ResetForNewAvatar`是明确的 Respawn Reset 边界：移除死亡/控制状态、按比例重置 Health/Shield、清理旧事件缓存并推进 AvatarGeneration/WorldContextGeneration。真正何时重生由 Gameplay/MOBA/项目层决定。
