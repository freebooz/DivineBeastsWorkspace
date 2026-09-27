# API（公开接口）

`IGamePlatformNavigationService（平台导航服务接口）`公开：`ProjectPoint（点投影）`、`TestPath（可达性测试）`、`FindPath（同步寻路）`、`FindPathAsync（异步寻路）`、`CancelRequest（取消请求）`、`FindRandomReachablePoint（随机可达点）`、Invoker 注册/撤销和 Diagnostics（诊断）。

`UGamePlatformNavigationWorldSubsystem（导航世界子系统）`是当前 ServerOnly 实现，每个 UWorld（世界）独立实例并持有独立 WorldGeneration（世界代次）。

Request 只传稳定 `AgentProfileId`和 `FilterId`，不允许外部上传任意 `TSubclassOf<UNavigationQueryFilter>`类路径。Async 请求使用 GUID RequestId，不使用数组下标身份。

客户端共享模块仅提供契约和 `Advisory（建议性）`语义；本轮没有提供客户端权威 Server Navigation Service 实现。
