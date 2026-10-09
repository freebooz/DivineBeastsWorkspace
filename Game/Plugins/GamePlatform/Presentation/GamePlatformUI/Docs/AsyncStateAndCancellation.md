# AsyncStateAndCancellation（异步状态与取消）

OpenScreenAsync（异步打开页面）返回 `FGamePlatformUIAsyncRequest`，包含 RequestId、ScreenId 和 Generation（代次）。PendingRequests（待处理请求）是回调有效性的第一道门。

CancelOpen（取消打开）先删除PendingRequest/临时ViewModel与成功资格；一般请求立即释放本调用者Data租约，正在同步构造控件的请求则保留UI自有租约到AddWidget返回、撤回控件后再释放。迟到回调和构造返回均重验代次及Pending资格，不能恢复取消请求。

ViewModel 另有 Revision/PageGeneration，用于业务异步状态的新鲜度判断。页面关闭后 PageGeneration 变化，旧回调必须被忽略。

LoadMap 前 `PrepareForTravel（准备跨地图）`统一取消未完成页面加载，并递增 Manager generation。

## 构造回调中的取消顺序（2026-09-30）

CommonUI AddWidget可能同步触发页面激活/切栈。此时CancelOpen先删除Pending资格，不提前释放UI持有的构造资源租约；AddWidget返回后按请求/完整租约/布局及世界代次重验，失效则先同步撤回新控件，再释放本次租约，不登记Active或广播Opened。一般非构造请求仍立即撤销自有需求。Data世界清理会独立终止世界租约，本地保留不授予退出后成功资格。
