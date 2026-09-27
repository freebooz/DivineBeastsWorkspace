# TasksServicesEvaluators（任务、服务与评估器）

第一版自定义 BehaviorTree Task 只有 `UBTTask_GamePlatformAITryAttack`：取得 `AGamePlatformAIController`并调用服务器 `TryAttackCurrentTarget`，成功/失败明确返回。没有创建只包装原生 Wait/MoveTo 的冗余 Task。

Patrol/Chase/ReturnHome 使用 Controller更新 GoalLocation 和中立 Intent，正式 BT 资产应优先使用 UE 原生 Move To、Wait、Blackboard Decorator。

当前没有自定义 BT Service、EQS Service 或 StateTree Evaluator；StateTree未实现，因此不存在空 Task/Evaluator。

异步资产所有权由 Controller持有 StreamableHandle，UnPossess/EndPlay取消，Generation不匹配的回调被忽略。
