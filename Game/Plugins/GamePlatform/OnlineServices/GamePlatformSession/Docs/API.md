# D03｜实际接口与调用边界

**当前没有IGamePlatformSessionService、GamePlatformSessionServices或公开子系统入口。** JoinDefaultWorld、JoinAssignment、TransferSession、ReconnectSession、LeaveSession等真实业务API尚未接通。Online目录为空，不能编造其签名、偷取令牌或补一套认证来满足表面接口数量。本节记录已经编译的内部算法接口，不承诺稳定跨插件ABI。

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

当前没有状态订阅/撤销、UObject弱调用者或通用重试服务，因此不提供伪造的订阅/解绑示例。未来公开服务返回结果须复用FGamePlatformResult，并与Core的取消/失败语义对齐，不将这里的内部EAcceptance发布成第二套跨语言错误协议。

已编译的最小内部调用样例见[SessionStateTests.cpp](../Source/GamePlatformSession/Private/Tests/SessionStateTests.cpp)中的Begin和Connect：创建独立State → SetAuthentication → Begin → Assign → CommitTravel → Observe四事实 → Snapshot。该文件已在CMake Debug/Release执行。样例内的事实是测试输入，不能复制到主工程作为“真实连接成功”逻辑。真实加入/迁移/重连/离开/订阅业务C++样例待Online与引擎适配实现后提供。
