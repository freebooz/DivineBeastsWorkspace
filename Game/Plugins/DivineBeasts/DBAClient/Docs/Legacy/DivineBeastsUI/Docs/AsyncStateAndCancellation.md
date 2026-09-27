# AsyncStateAndCancellation（异步状态与取消）

每个UI命令由ViewModel生成：

- RequestId
- PageGeneration
- ExpectedRevision

ViewModel记录Pending Request，并在页面Deactivated（停用）时尝试通过Command Port取消。当前部分业务Owner尚无统一Cancel接口，因此CancelUICommand可能返回false；即便无法物理取消，旧回调仍通过平台ViewModel的Revision/PageGeneration校验忽略。

组合根在命令提交前比较 ExpectedRevision 与当前 UIState.Revision，不一致立即返回 UI.Command.StaleView。

UI防双击、页面禁用只能减少重复Intent，真正的角色创建等业务幂等仍由OperationId/Server（操作ID/服务器）保证。

页面关闭必须解绑状态Delegate，避免旧页面继续接收新账号/新世界回调。
