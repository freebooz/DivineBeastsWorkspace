# OutcomeAndBackend（结果与业务后端）

新增业务后端接口：无。新增 Go 微服务：无。实时交互权威由 UE Dedicated Server（专用服务器）执行。

当前世界 Outcome：Door Toggle、Pickup Consume、Harvest RemainingCharges；这些只保证当前 UE 服务器实例中的权威状态，不保证服务器重启、跨 Shard、永久背包、永久任务或货币。

未来永久奖励应由 Inventory/Quest/World Persistence 等正式领域通过服务器端 Outcome Handler 接入，并使用 InteractionCommitId/幂等键；本轮没有为了测试创建错误归属的 InteractionService。

完整网络链未来应复用 Identity/Online → Session/Server Control → Gameplay Player Active → Character → Interaction。当前 Online/Session 前置链和 UE 工具链没有真实联调证据，因此后端复用链验证状态为未执行。
