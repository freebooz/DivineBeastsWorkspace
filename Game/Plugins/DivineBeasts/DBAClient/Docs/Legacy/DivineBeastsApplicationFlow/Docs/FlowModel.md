# FlowModel（流程模型）

项目节点：DBA.Flow.Boot、Initialize、Authentication、LoadProfile、LoadRoster、CharacterEntry、CreateCharacter、ValidateSelection、RequestWorld、TransferWorld、WorldReady、InWorld、Recovering。

节点注册到 UGamePlatformApplicationFlowSubsystem（平台流程子系统）；项目层不保存第二套 CurrentState。

异步操作均通过平台 BeginOperation 取得 FlowRunId + NodeId + NodeGeneration + OperationId，并在回调时调用 IsOperationCurrent 拒绝旧流程、旧节点和旧请求回调。
