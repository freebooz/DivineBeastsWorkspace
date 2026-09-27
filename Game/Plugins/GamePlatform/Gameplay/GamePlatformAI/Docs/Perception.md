# Perception（感知）

服务器 AIController 使用 `UAIPerceptionComponent`。Sight（视觉）必配；Hearing（听觉）和 Damage Sense（伤害感知）由 AIDefinition Profile 可配置开关。Affiliation 暂时全接收，再由中立 Target Eligibility 二次过滤，不使用已取消的阵营体系。

Sight Stimuli Source 由 ServerOnly WorldSubsystem 对拥有 `UGamePlatformAITargetComponent`的 Actor 事件驱动登记；世界初始化时只做一次补登记，不进行每帧 GetAllActorsOfClass。
Development主工程同时提供不带 `UGamePlatformAIStateComponent` 的 `AFoundationAITestTargetCharacter（基础AI测试目标角色）`，它只携带AITarget/ASC/Combat，用于未来专服验证“AI感知非AI玩家目标”而不会被AIServer自动赋予AIController。

`OnTargetPerceptionUpdated`更新 Seen/Heard/DamageSource、LastKnownLocation、LastSensedTime 与 Target Generation。候选有 MemorySeconds 和 MaxCandidates 上限，Actor销毁后立即移除。

Hearing 开启时，“听见但未看见”的目标进入 Investigating，GoalLocation 使用 LastKnownTargetLocation；Sight重新获得后转 Chase/Attack。Damage Event 还会从 GamePlatformCombat 的中立 CombatEvent 把攻击来源加入候选。
