# Development（AI开发验证资源）

此目录用于 Unreal Editor（虚幻编辑器）真实创建 AI Development（AI开发验证）资产。计划资产：

- `DA_AI_FoundationMelee（基础近战AI定义资产）`。
- `BB_AI_Foundation（基础AI黑板资产）`，Key：TargetActor、LastKnownTargetLocation、HomeLocation、HasLineOfSight、IsDead、CanAttack、GoalLocation。
- `BT_AI_Foundation（基础AI行为树资产）`，使用原生 Move To / Wait / Blackboard Decorator，并可调用 `UBTTask_GamePlatformAITryAttack（平台AI尝试攻击任务）`。
- 可选测试 Blueprint Character/Controller 只在 Editor 确有必要时创建；当前 C++ `AFoundationAITestCharacter（基础AI测试角色）`已经提供共享Pawn骨架。

当前 Runner 没有可用且锁定的 UE5.8 工具链，不能通过文本工具伪造 `.uasset/.umap`。因此上述真实 BT/BB/DA 资产、NavMesh 测试地图、BehaviorTree运行、Dedicated Server（专用服务器）和 Cook 证据均保持未执行。

StateTree（状态树）第一版明确不实现，不创建空 `ST_AI_Foundation`冒充支持。
