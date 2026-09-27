# PerformanceAndScalability（性能与伸缩）

服务 Diagnostics 记录 SyncQueryCount、AsyncQueryCount、CancelledRequestCount、OutstandingAsyncRequests 和 RegisteredInvokers。配置限制最大异步请求数、最大路径距离和 Invoker 半径。

同步 FindPath/TestPath 设计为低频明确调用，不为每 AI 每帧路径查询。AI Patrol 只在状态/定时条件下请求随机点；Chase 的实际移动继续使用 BehaviorTree/PathFollowing 自身刷新机制。

Dynamic Modifier、NavMesh Tile Rebuild、SmartLink、Invoker 与并发 Path Query 的真实 CPU/GameThread/内存开销尚未测量。

1/10/20+ Agent 压力验证脚本已执行入口，但因缺少 UnrealEditor-Cmd 返回 not_executed；不得由静态计数推断生产服务器容量。
