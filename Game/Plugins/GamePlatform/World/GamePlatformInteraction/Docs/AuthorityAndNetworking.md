# AuthorityAndNetworking（权威与网络）

Begin/Cancel Server RPC 定义在玩家拥有 Actor 上的 `UGamePlatformInteractorComponent`。目标 Door/Pickup/Harvest 不承载客户端主请求 RPC。UE5.8 的 Actor Component RPC 会随拥有 Actor 的 Owning Connection（拥有连接）执行。

Interactor、Target 状态和 Option 规则由服务器重新读取；客户端不能决定距离、LOS、HoldDuration、Concurrency、Reward、Commit 或最终世界状态。

CurrentSession/LastResult 使用 OwnerOnly（仅所有者）复制；TargetInstanceId/Generation/Revision、Availability、Pickup/Harvest/Door状态和 Occupancy 复制给相关客户端，持续状态用于 Late Join 恢复。

2026-10-31 补齐成功事实的拥有者通知：`PublishEvent`构造唯一的不可变Event，并通过`ClientReceiveCommittedInteraction`可靠Client RPC携带对应Session快照送到远端拥有连接。终态开始时先捕获原Controller/Pawn；结果监听重入后只有拥有关系仍匹配才排队消息，不能把旧成功重定向给新玩家。Listen/Standalone的本地权威仍只收到原服务器广播。

客户端在实际Begin RPC发送前保存原始请求，收到成功消息时核对Request/Session/Option/Result全部身份、目标及World、原Controller/Pawn和本地作用域；不能借用鼠标焦点或当前会话重建旧目标。只有已发送且仍有效的本地请求才投影到既有`OnInteractionEvent`，并在外部回调前去重。发送记录和Session去重历史各最多64条，旧记录被淘汰后迟到消息直接丢弃。失败/取消、首次快照和持续状态仍使用既有OwnerOnly属性，不从RepNotify重新推导一次性成功。

这条RPC只传输成功事实，不重新执行Commit、不签发资格，也不代表外部业务后端已完成后续操作。尚无本轮真实RPC传输或联机通过证据。

真实 Dedicated Server + 双客户端、断线、延迟、重排序与 Late Join 仍需 UE5.8 工具链和测试地图后执行。
