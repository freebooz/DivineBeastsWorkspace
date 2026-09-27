# NavigationModel（导航模型）

`FGamePlatformNavigationRequest`包含 RequestId、WorldGeneration、AgentProfileId、Start、Goal、FilterId、PartialPathPolicy、TimeoutSeconds、OwnerScope和 Authority。坐标统一为 UE 厘米。

`FGamePlatformNavigationPathResult`包含 Status、Error、RequestId、WorldGeneration、真实 Engine PathPoints、PathLength、PathCost、Partial、ReachedGoal、ResolvedStart/ResolvedGoal。PathLength 是厘米；PathCost 是导航成本，两者不会混用。

`FGamePlatformNavigationRequestHandle`由 GUID RequestId + WorldGeneration 组成，可跨异步回调验证，不依赖容器索引。

错误枚举区分 NavigationUnavailable、NavDataMissing、InvalidAgentProfile、ProjectionFailed、PathNotFound、PartialPathRejected、InvalidFilter、Cancelled、Timeout、OwnerDestroyed、WorldTearingDown、SmartLink/Invoker 等，不统一压成 NavigationFailed。
