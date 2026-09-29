# PerformanceAndSecurity（性能与安全）

Interactor/Interactable 都关闭 Tick。Focus 使用本地有界定时 Trace；Active Hold 使用服务器有界周期验证；没有 GetAllActorsOfClass 每帧扫描、没有 Focus RPC、没有每帧后端调用。

Begin/Cancel RPC（开始/取消远程调用）为低频 Reliable（可靠）请求，服务器使用彼此独立的时间窗限流。Recent Request（最近请求）同时受 MaxRecentRequests（最大数量）与 RecentRequestLifetimeSeconds（存活时间）约束；Target Commit（目标提交）结果缓存按 MaxRecentRequests 有界保留，并在 TargetGeneration（目标代次）推进时整体清空，因此不会无限增长。

客户端 Request（请求）不包含 MaxDistance、LOS、HoldDuration、Concurrency、Reward 或 Server Result（最大距离/视线/长按时长/并发/奖励/服务器结果）等权威规则。Begin（开始）阶段服务器严格重验组件实例身份、TargetGeneration、ObservedTargetRevision（观察修订号）、Option（选项）、Availability（可用性）、距离、LOS（视线）和 Gameplay（玩法资格）；会话成功占用后，Commit/Hold（提交/长按）持续重验 Session、Generation、Availability、距离、LOS 和资格，但不会因另一个 Shared（共享）会话正常提交导致 Revision 推进而误杀当前会话。真正会使活动会话失效的配置/可用性变化由 Authority API（服务器权威接口）显式取消。

实际 Trace 次数、Active Session 数、服务器验证耗时、RPC数量、CPU/内存尚未通过 Unreal Insights/网络 Profile 测量，因此不宣称“已优化最佳”。
