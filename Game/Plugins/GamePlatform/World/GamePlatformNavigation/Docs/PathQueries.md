# PathQueries（路径查询）

ProjectPoint 对 Position/Extent 做有限值与安全上限检查，使用当前 NavData 和 Filter 调用 UE `ProjectPointToNavigation`。

TestPath 使用 `TestPathSync`只返回可达性；FindPath 使用 `FindPathSync`返回引擎真实路径点、路径长度和成本。同步查询计入 Diagnostics，设计用途仅为低频明确请求/测试。

FindPathAsync 使用 UE5.8 `FindPathAsync`并保存 EngineQueryId、WorldGeneration、OwnerScope、Request、Completion 和 Timeout Timer。Cancel/Timeout 调用 `AbortAsyncFindPathRequest`；同时从本地 Request Registry 移除，因此即使底层回调迟到也会被忽略。

PartialPath 策略：RejectPartial 会拒绝部分路径；AcceptPartial/AcceptForPreviewOnly 允许返回 Partial 状态。调用方仍需按用途决定是否实际移动，Attack Chase 不应把墙边 Partial 当成到达目标。
