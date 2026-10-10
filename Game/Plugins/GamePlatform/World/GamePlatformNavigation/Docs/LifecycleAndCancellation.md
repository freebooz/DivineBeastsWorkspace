# LifecycleAndCancellation（生命周期与取消）

每个 UWorld Subsystem 使用独立 WorldGeneration；RequestHandle携带RequestId、WorldGeneration与正int64 OperationGeneration。FindPathAsync只为真实接纳请求签发代次；默认0或旧两字段手造句柄失效，调用方须保存原返回值，不能只重建ID。旧世界或同世界旧操作句柄不能取消新请求。

异步请求记录 EngineQueryId、OwnerScope 弱引用、Request、Completion 和 Timeout Timer。正常完成前若逻辑 Cancel/Timeout，会从 Registry 删除并调用 UE `AbortAsyncFindPathRequest`；迟到回调须匹配RequestId、WorldGeneration与OperationGeneration，否则丢弃。RequestId在完成后可以复用，旧A.Cancel明确返回false，B记录/Timer/Completion不受影响。

OwnerScope 曾存在但完成时弱引用失效，则结果不回调；WorldGeneration 不匹配同样丢弃。Deinitialize 会取消当前 Registry 请求并撤销 Invoker。

当前 OwnerDestroyed/WorldTearingDown 只形成安全丢弃/取消语义，没有真实 Travel/Streaming 运行日志；Multi-PIE 隔离依赖 UWorldSubsystem 作用域，运行证据未执行。

2026-10-09追加回归：现有原生生产策略测试完成A→同ID启动B→旧A取消拒绝→B仍完成，Debug/Release通过。Private/Tests/NavigationReusedHandleTests.cpp驱动真实世界服务的内部完成/取消记录；这只是无NavMesh的服务夹具，本轮未执行UE Automation或真实异步导航。公开反射字段新增而未重命名旧字段；旧句柄不提供降级取消，以免突破所有权。
