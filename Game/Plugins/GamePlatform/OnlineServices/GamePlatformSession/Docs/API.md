# D03｜实际接口与调用边界

当前公开入口为 `UGamePlatformSessionClientSubsystem`。它提供 `BeginOperation / BeginTransfer / Reconnect / CancelTransfer / LeaveSession / NotifyDisconnected / ResolveRemoteState / ReportLocalFact`、只读Snapshot和事件订阅，并通过 `IGamePlatformSessionTransport` 隔离真实网络动作。Transport尚无可安全装配的生产实现：缺失可信完整Binding时必须返回 `SessionTransportUnavailable`，不得伪造连接成功。

实际头文件：[SessionConnectionState.h](../Source/GamePlatformSession/Private/State/SessionConnectionState.h)。所有方法要求同一调用线程串行访问；未来适配器应使用游戏线程。字符串均为非敏感身份，不接受访问令牌、准入材料或凭据URL。返回EAcceptance表示同步处理结果，不表示HTTP或UE异步操作完成。

```cpp
FSessionConnectionState(std::string ScopeId);
EAcceptance SetAuthentication(std::string AuthSessionId, std::uint64_t Generation);
EAcceptance Begin(EIntent Intent, std::string OperationId, std::string AttemptId,
    double NowSeconds, double DeadlineSeconds, FOperationIdentity& OutIdentity);
EAcceptance Assign(const FOperationIdentity& Identity, const FBinding& Binding, double NowSeconds);
EAcceptance CommitTravel(const FOperationIdentity& Identity, double NowSeconds);
EAcceptance Observe(const FOperationIdentity& Identity, const FBinding& Binding, EFact Fact, double NowSeconds);
EAcceptance Cancel(const FOperationIdentity& Identity, double NowSeconds);
EAcceptance Fail(const FOperationIdentity& Identity, double NowSeconds);
EAcceptance ResolveRemote(const FOperationIdentity& Identity, const FBinding& ConfirmedBinding);
void AdvanceDeadline(double NowSeconds);
bool Disconnect(const FBinding& Binding);
void Leave();
FSnapshot Snapshot() const;
```

- 构造要求独立ScopeId；空身份不会允许Begin。SetAuthentication要求非零单调账号代次，同代次换账号拒绝；正常刷新不需要改变代次。空AuthSessionId表示退出，后续Begin拒绝。
- Begin只接收Join/Transfer/Reconnect意图。Join不能覆盖当前Ready，Transfer要求有来源；同活动OperationId/AttemptId/Intent返回同句柄，不延长Deadline，不同操作返回Busy。Deadline是外部单调秒数、有限值且大于Now，不是World时间。失败清空OutIdentity。
- Assign接受上层已认证和校验的完整Binding。内部不能证明授权；缺字段/零Epoch拒绝，迁移Epoch必须高于当前来源。Pending与Current分离。
- CommitTravel在真正调用引擎前记录不可回滚边界，清空来源。它自身不调用ClientTravel。调用者必须先安装关联监听和超时。
- Observe核对完整操作身份和Binding，聚合网络连接、准入提交确认、目标世界加载及控制器就绪四事实；全部满足才完成一次。错误世界/实例/协议/Epoch、旧账号或旧尝试不能推进。它没有网络认证能力，不能暴露给蓝图或非可信事件源。
- Cancel/Fail/AdvanceDeadline只收敛本地算法。分配响应丢失也可能已经占位，所以设置远端查询屏障；切服后取消结果为Uncertain。ResolveRemote仅接受当前已结束操作的后端查询结果，不能用后端Binding伪造本地新连接Ready。
- Disconnect要求确切Current，旧来源离开不会清新目标。Leave清本地连接并保留认证，但不实际关闭网络或调用后端。Snapshot为独立值，生命周期不依赖状态对象；CompletionCount可用于测试终态次数。

公开Subsystem已经通过 `OnSessionChanged` 发布值Snapshot；异步Transport回调统一使用弱UObject并切回游戏线程。公开结果继续复用 `FGamePlatformResult`，内部 `EAcceptance` 不跨模块发布。Snapshot显式暴露Intent、RecoveryState、bRecoveryRequired和bCanRetry，使ApplicationFlow无需猜测私有状态。

状态内核测试见[SessionStateTests.cpp](../Source/GamePlatformSession/Private/Tests/SessionStateTests.cpp)；公开契约边界测试见 `Private/Tests/GamePlatformSessionContractTests.cpp`。测试覆盖Intent、恢复屏障、Epoch防旧、旅行前/后超时差异和非法Fact拒绝，但测试事实不是UE网络事实。真实加入/迁移必须等待可信Binding和生产Transport后再增加E2E证据。
