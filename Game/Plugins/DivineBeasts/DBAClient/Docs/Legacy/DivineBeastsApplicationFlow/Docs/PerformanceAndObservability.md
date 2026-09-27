# PerformanceAndObservability（性能与可观测性）

ApplicationFlow不做每Tick轮询。认证、Profile、Roster、Session和Loading均通过事件或异步完成回调推进。

流程遥测建议仅记录中立聚合：Flow.Node.Duration、Flow.Node.Failed、Login.Duration、Profile.Load.Duration、Character.Create.Result、WorldAssignment.Duration、Travel.Duration、WorldReady.Duration、Reconnect.Attempt。

Attributes（属性）仅包含NodeId、Result、ErrorCode、Experience和ServerRole等非敏感值。

禁止上报Password、Token、TransferTicket和默认CharacterName。

当前没有真实UE Client压力、后端端到端延迟或Multi-PIE性能证据，因此性能基线状态为“未执行”。
