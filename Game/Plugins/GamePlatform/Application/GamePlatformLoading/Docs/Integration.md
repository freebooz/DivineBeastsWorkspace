# 集成边界与实际接线

## Data

`FLoadingDataTask`通过`IGamePlatformDataService::Get`调用真实`AcquireDefinition`，使用Instance期租约和当前GameInstance作为弱调用者。调用者自身失效由Loading操作检测并清理。异步完成保存本尝试结果；实时成功还要求`GetLeaseState==Succeeded`且`GetLoadedDefinition`非空。失败/取消只调用本租约的ReleaseDefinition，不卸载其他世界资产。

一项Data任务对应一份定义及Bundle需求。多定义用多任务以公开部分失败边界。本轮Foundation请求现有Probe和Flow两份定义，不新造第二个AssetManager，也没有资源路径LoadObject旁路。

## Flow与主工程

主工程`DBAFoundationCoordinator.cpp`的既有`Ready`工厂现返回`UDBALoadingFlowNode`，其他节点保持原职责。节点从Context取得Flow Handle、NodeId和NodeGeneration；Loading完成订阅另核对OwnerScopeId、OperationId、Generation。仅匹配当前节点且仍活跃时调用公开`UGamePlatformApplicationFlowSubsystem::SubmitEvent`，由Flow既有排队机制处理，回调不直接推进流程。

项目仍负责切图和基础观察者。`CheckWorld`只有在原项目`IsFoundationReady`确认当前切图操作、目标世界、本地控制器/Pawn和Probe可读时，才向Loading报告世界可操作。Loading再次核对世界身份和生命周期。节点Finish在成功、取消、失败时均先失活，停世界采样，撤Loading订阅，再释放操作。旧Flow事件仍由Flow的节点代次拒绝。

成功后Loading临时Probe/Flow需求归还，项目组合根原先持有的独立Probe租约、流程持有的定义租约不受影响。开发HUD尚未增加Loading专属进度面板；已有HUD仍展示流程诊断。UI消费方可使用公开值快照，不能读取私有Subsystem。

## Session与Online

Session当前只有私有状态算法，无可消费的公开连接/准入快照或已验证服务器链。本轮不从它的私有状态推导成功、不读取票据、不调用HTTP、不Travel、不重连。`SessionReady`缺工厂直接拒绝。Online并行开发内容不作为本轮已经联调的证据。

后续真实适配应在客户端组合层登记任务，并核对SessionId、目标InstanceId/BootId、World身份、ConnectionGeneration、权威准入与当前操作绑定；断开/失败必须传播。双端Loading不直接依赖客户端Session。该部分及FoundationSessionLoading均未执行，不能以测试Task替代。

## 《神兽联盟》推荐Loading事实图

平台层不新增神兽联盟专属任务类型；由 `DBAClient（神兽联盟客户端组合插件）` 在真实公共服务就绪后注册项目适配任务。推荐把一次 `EnterWorld（进入世界）` 的 Loading Operation（加载操作）拆成：

```text
SessionAdmission        # Required：真实Session/服务器准入，当前前置未完成
        ↓
WorldDefinition         # Required：GamePlatformData定义与必要Bundle
        ↓
WorldPresence           # Required：当前目标世界已BeginPlay且项目声明基础可操作
        ↓
CharacterReady          # Required：Pawn/角色初始化、必要GAS能力与权威身份就绪
        ↓
GameplayReady           # Required：当前Experience/Gameplay规则和基础交互可用
        ↓
EssentialUIReady        # 客户端通常Required：根HUD/输入相关基础界面可用

NonCriticalPresentation # Optional/Degradable：非关键VFX/SFX/装饰性表现
```

OpenWorld（常驻世界）、Village（新手村）和 MainArena（主竞技场）可以复用同一机制，但由项目层提供不同 OperationSpec（操作规格）和事实适配器。Loading 不负责 Travel（切图）、服务器分配、角色生成、GAS授权或 UI 创建，只验证这些所属系统报告的当前事实。MainArena 的 1v1～5v5 模式也不进入 Loading 平台规则。
