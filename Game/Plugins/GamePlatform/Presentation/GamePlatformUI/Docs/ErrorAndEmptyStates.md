# ErrorAndEmptyStates（错误与空状态）

平台基类不硬编码项目业务状态，但要求上层 ViewModel 能明确表达 Loading、Ready、Empty、Disabled、Error、Retry、NoPermission、Submitting 等状态。

错误面向用户时使用 FText（本地化文本）；内部 ErrorCode（错误码）应在客户端映射，Shipping（正式发布）不直接显示堆栈、URL、服务地址、Token 或服务器内部原因。

页面资源加载失败通过 `OnScreenOpenFailed`返回 RequestId、ScreenId 和用户可展示 Reason。

空列表与错误列表必须是两个不同状态，不能把网络失败渲染成“暂无数据”。
