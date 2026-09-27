# LifecycleAndCancellation（生命周期与取消）

每个 UWorld Subsystem 使用独立 WorldGeneration；RequestHandle 同时携带 RequestId 和 WorldGeneration。旧世界 Handle 不能取消/控制新世界请求。

异步请求记录 EngineQueryId、OwnerScope 弱引用、Request、Completion 和 Timeout Timer。正常完成前若逻辑 Cancel/Timeout，会从 Registry 删除并调用 UE `AbortAsyncFindPathRequest`；迟到回调因为 Request 不存在而被丢弃。

OwnerScope 曾存在但完成时弱引用失效，则结果不回调；WorldGeneration 不匹配同样丢弃。Deinitialize 会取消当前 Registry 请求并撤销 Invoker。

当前 OwnerDestroyed/WorldTearingDown 只形成安全丢弃/取消语义，没有真实 Travel/Streaming 运行日志；Multi-PIE 隔离依赖 UWorldSubsystem 作用域，运行证据未执行。
