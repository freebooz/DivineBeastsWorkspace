# BlackboardAndState（黑板与状态）

标准 Key：TargetActor、LastKnownTargetLocation、HomeLocation、HasLineOfSight、IsDead、CanAttack、GoalLocation。客户端不复制完整 Blackboard。

公开状态枚举：Idle、Moving、Investigating、Chasing、Attacking、Controlled、Dead、Disabled；状态组件复制 `FGamePlatformAIStateSnapshot`，用于 Late Join 和后续表现层消费。

StateRevision 每次公开状态/目标/Intent/Generation变化递增。AIEntityId 由服务器生成，CurrentTarget 只复制稳定 EntityId，而不复制服务器候选表。

Death 时 Blackboard IsDead=true；RespawnReset 时清除 Target/CanAttack/旧感知候选并恢复 IsDead=false，避免上一生命的敌人残留。
