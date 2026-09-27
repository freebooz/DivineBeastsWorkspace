# SessionsAndConcurrency（会话与并发）

服务器创建 SessionId，记录 RequestId、InteractorGeneration、Target三元组、OptionId、Mode、ServerStartTime、RequiredDuration 和 Result。Hold 进度基于 GameState ServerWorldTime（服务器世界时间）。

Exclusive（独占）已实现：同一 Target/Option 被占用时第二个 Session 在 TryAcquire 阶段返回 TargetBusy。Shared（共享）支持 MaxConcurrent 基础并发占用，但任何会改变 TargetRevision 的 Commit 会使其它旧 Revision Session 在后续验证时变为 Stale；这是第一版保守一致性策略。

Hold 使用服务器定时器按 `HoldValidationInterval`重新验证 Gameplay资格、Generation、Revision、Option、距离和 LOS；满足服务器持续时间才 Commit。

用户主动 Cancel、Target EndPlay、Interactor EndPlay、LevelTransition、Generation/Revision失效、超距、LOS丢失都会阻止迟到 Commit。
