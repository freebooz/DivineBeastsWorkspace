# D04｜实际状态、代次与事件

实际枚举由SessionConnectionState.h唯一维护。Idle、RequestingAssignment、PreparingConnection、Connecting、AwaitingAdmission、Ready、Transferring、Reconnecting已被算法使用；Leaving为预留观察语义，当前Leave同步收敛Idle，未实现可观察的异步离开阶段。不得据枚举存在声称阶段已接入引擎。

合法主转换：Idle经Begin(Join)进入RequestingAssignment；Ready经Begin(Transfer)进入Transferring；无活动操作时Begin(Reconnect)进入Reconnecting。Assign只接受首次完整目标或完全相同的重复目标，随后PreparingConnection。CommitTravel进入Connecting并清除来源。四事实可以任意次序抵达，Observe进入AwaitingAdmission，集齐后Current=Pending并Ready。事实次序不规定引擎PreLogin与地图加载先后。

无认证、跨作用域、旧账号/连接代次、不同AttemptId、错误Binding、未旅行就报告事实、不确定远端状态尚未查询时发起新操作，均拒绝。重复活动操作不延长超时，终态后的回调不再次完成。Clock由调用者提供单调秒数；没有跨图Ticker或外部引擎委托接线。

各阶段仅持有非敏感描述：分配中持有操作身份和截止时间；准备中额外持有Pending及仍有效的来源Current；旅行后清除Current，只保留Pending和就绪事实；完成后保留Current、终态和完成计数；结束时清除Pending和事实。资源加载、HTTP请求、票据字节、定时器与NetDriver的释放仍需适配器实现。

五条时序如下，箭头仅表示内核调用顺序，**不是已完成真实联调**：

1. 首次加入：认证身份→Begin(Join)→已验证Assign→CommitTravel→四事实Observe→Ready。
2. 迁移成功：Ready(A)→Begin(Transfer)保留A→Assign(B且Epoch递增)→CommitTravel断开来源语义→四事实(B)→Ready(B)。旧A Disconnect因Binding不同拒绝。
3. 迁移失败：旅行前Cancel保留A并等待远端撤销查询；ResolveRemote只能确认同一个仍有本地连接的A。旅行后Cancel清本地并返回Uncertain，不伪称回滚；必须查询/释放，再以新操作恢复。
4. 重连：Disconnect清本地→后端状态查询完成→Begin(Reconnect)→新Assignment/Attempt→四事实。尚未实现自动退避、预算或新凭据获取，当前Begin不会自动重连旧地址。
5. 退出/换账号：Leave终止活动操作、清本地，远端释放独立确认；SetAuthentication新代次使旧回调失效，不读取原账号令牌。旧账号远端绑定仍需服务端撤销/租约清理，内核不证明这些已经执行。

取消与后端提交竞争由数据库状态决定：Cancel先赢则Released阻止Claim/Commit，Commit先赢则Cancel返回Admitted。客户端必须把这种结果转入查询/Leave，不映射成简单取消成功。权威Epoch和租约是数据库约束，真实UE服务器执行权威租约的代码尚缺，不能保证游戏侧双活已经解决。
