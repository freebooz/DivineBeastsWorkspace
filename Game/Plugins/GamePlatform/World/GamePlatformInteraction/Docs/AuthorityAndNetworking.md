# AuthorityAndNetworking（权威与网络）

Begin/Cancel Server RPC 定义在玩家拥有 Actor 上的 `UGamePlatformInteractorComponent`。目标 Door/Pickup/Harvest 不承载客户端主请求 RPC。UE5.8 的 Actor Component RPC 会随拥有 Actor 的 Owning Connection（拥有连接）执行。

Interactor、Target 状态和 Option 规则由服务器重新读取；客户端不能决定距离、LOS、HoldDuration、Concurrency、Reward、Commit 或最终世界状态。

CurrentSession/LastResult 使用 OwnerOnly（仅所有者）复制；TargetInstanceId/Generation/Revision、Availability、Pickup/Harvest/Door状态和 Occupancy 复制给相关客户端，持续状态用于 Late Join 恢复。

真实 Dedicated Server + 双客户端、断线、延迟、重排序与 Late Join 仍需 UE5.8 工具链和测试地图后执行。
