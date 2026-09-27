# InteractionModel（交互模型）

Target 身份由三元组组成：TargetInstanceId（实例身份）、TargetGeneration（生命周期代次）、TargetRevision（可交互状态修订）。服务器生成 InstanceId，客户端只能回传观察值。

Session 状态：Requested/Validating/Active/Committing/Completed，以及 Rejected/Cancelled/Failed/TimedOut。Instant 和 Hold 共用同一状态模型。

Request 去重使用有界 RecentRequestResults + 顺序队列，完成请求返回同一终态；仍在 Active 的同 RequestId 返回当前 Session，不重新 Commit。目标 Commit 也以 SessionId 本地幂等，并有有界历史缓存。

当前世界 Outcome 只覆盖 Toggle、Consume、Harvest 和自定义接口 Commit；不包含永久背包、任务、货币或跨服务器世界持久化。
