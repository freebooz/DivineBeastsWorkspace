# AsyncStateAndCancellation（异步状态与取消）

OpenScreenAsync（异步打开页面）返回 `FGamePlatformUIAsyncRequest`，包含 RequestId、ScreenId 和 Generation（代次）。PendingRequests（待处理请求）是回调有效性的第一道门。

CancelOpen（取消打开）会取消 Streamable Handle 并删除 PendingRequest/临时 ViewModel；即使底层回调迟到，也会在入口发现 RequestId 已不存在而直接返回。

ViewModel 另有 Revision/PageGeneration，用于业务异步状态的新鲜度判断。页面关闭后 PageGeneration 变化，旧回调必须被忽略。

LoadMap 前 `PrepareForTravel（准备跨地图）`统一取消未完成页面加载，并递增 Manager generation。
