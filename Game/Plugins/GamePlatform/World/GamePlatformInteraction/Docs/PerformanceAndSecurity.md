# PerformanceAndSecurity（性能与安全）

Interactor/Interactable 都关闭 Tick。Focus 使用本地有界定时 Trace；Active Hold 使用服务器有界周期验证；没有 GetAllActorsOfClass 每帧扫描、没有 Focus RPC、没有每帧后端调用。

Begin/Cancel RPC 为低频 Reliable，服务器按时间窗限流；Recent Request 和 Target Commit 历史均有最大数量/过期策略，避免无限增长。

客户端 Request 不包含 MaxDistance、LOS、HoldDuration、Concurrency、Reward 或 Server Result。Target Commit 前再次检查 Session、Generation、Revision、Availability、距离、LOS和Gameplay资格。

实际 Trace 次数、Active Session 数、服务器验证耗时、RPC数量、CPU/内存尚未通过 Unreal Insights/网络 Profile 测量，因此不宣称“已优化最佳”。
