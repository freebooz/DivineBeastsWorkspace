# ErrorRecoveryAndCancellation（错误恢复与取消）

项目错误模型区分认证、Profile、Roster、Create、Selection、Experience、Assignment、Ticket、Travel、Admission、WorldReady、Session、Reconnect、Contract和StaleOperation。

所有异步回调通过 FGamePlatformFlowOperationToken（平台流程操作令牌）校验 FlowRunId、NodeId、NodeGeneration、OperationId；旧回调直接忽略，不推进新流程。

重启流程、Logout/Account Switch、Deinitialize都会取消HTTP请求、Loading Operation和Session Transfer，并InvalidateRun（使流程运行失效）。

TransferTicket失败/过期/结果未知时不复用旧票据，重新请求Assignment。
