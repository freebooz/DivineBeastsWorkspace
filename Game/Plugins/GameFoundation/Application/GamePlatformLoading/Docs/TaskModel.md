# 任务模型与工厂

实际任务接口是`IGamePlatformLoadingTask`：`Start(UGameInstance&, const FGamePlatformLoadingTaskSpec&) -> FGamePlatformResult`、`Poll() -> FGamePlatformLoadingTaskUpdate`、`Release() -> void`。每次尝试工厂返回新的`TUniquePtr`，没有任意字符串执行命令。Start失败也必须可Release；Release负责取消/解绑并归还自己申请的资源。

内置类型只有`Data`与`WorldPresence`。它们不能被覆盖或撤销。`SessionReady`没有内置实现：无注册时返回SessionPrerequisiteMissing，未来必须以真实公开服务适配器接入。`TestLatch`仅在Private/Tests里显式注册，不在生产默认流程中。

生产任务状态：Waiting → Running → Succeeded/Failed；操作取消时未终态任务变Cancelled。Degradable首个失败会回到Waiting，执行代次加一并启动独立Fallback；回退成功记Degraded，回退失败否决操作。原始失败错误保留用于诊断。

启动前以有界拓扑消除算法校验完整DAG：空图、超256任务、空身份、重复身份、未知/重复依赖、循环、非法权重和时间全部拒绝。图冻结，不允许运行中插入任务改变权重分母。所有依赖必须Succeeded或Degraded才能启动下游；可选父任务失败不能作为必需子任务的满足条件。

执行令牌包含操作代次、TaskId、尝试代次，不使用对象地址或数组下标。对运行任务只接纳一次终态；旧操作或原始尝试完成不能推进新操作/回退。UE门面另外核对ScopeId和OperationId。任务结果采样后先经该生产内核，再发布快照。

工厂注册使用作用域与RegistrationId；不使用裸类型名作为撤销权限。已有未释放操作时工厂集合不可变，因此不存在加载中撤销后跳转到另一个实现的行为。注册重复失败，必须先完成清理再撤销。注册者需要在自身模块卸载前撤销，不能把卸载后的函数对象留在实例中。
