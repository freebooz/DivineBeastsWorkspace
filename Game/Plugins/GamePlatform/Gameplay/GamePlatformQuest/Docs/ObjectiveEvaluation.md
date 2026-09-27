# ObjectiveEvaluation（目标评估）

QuestEvent 到达后先按 EventType 找到该玩家相关 Objective，再按 QuestId 分组，一次事件可同时更新同一 Quest 的多个 Objective，也可合法推进多个不同 Quest。

过滤规则：EventType 必须一致；Event.SemanticTags 必须包含 Objective.RequiredSemanticTags；Region Objective 额外要求 RegionId 一致。Interaction/Combat 语义标签由项目服务器适配器从服务器目标 Actor Tags 映射已注册 GameplayTag。

Count 每次 +1；Sum 使用非负 NumericValue；SetComplete 直接达到 RequiredValue。CurrentValue 不超过 RequiredValue，负值不会减少进度。Optional Objective 不阻止 Quest 完成。

同一个 EventId 的内存去重作用域是 QuestId + EventId，而不是玩家全局 EventId；这样同一个可信 Combat/Interaction 事实可以推进多个 Quest，但不会在同一个 Quest 中重复累计。
