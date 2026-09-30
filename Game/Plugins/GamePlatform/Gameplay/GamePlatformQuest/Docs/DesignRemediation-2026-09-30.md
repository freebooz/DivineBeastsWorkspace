# F04 任务事件事务和显示序列（2026-09-30）

共享模块只持定义、快照和值规则；客户端只读取服务器快照；服务器世界服务消费项目可信事件及异步 Persistence Port。写进度和事件去重由服务器负责，客户端不能提交权威进度。所有 UObject/服务操作限定游戏线程；异步端口完成继续核对玩家运行身份。

对账先完整校验 Loaded 到临时 Quests，定义不兼容、空QuestId、重复QuestId均失败，旧进度/已接纳事件不动。成功校验后，先为全部未落库载荷建立重放所有权再撤销旧 PendingEventPayloads/PendingEventIds。有限 DeferredEvents 满时不部分插入，不丢载荷：整组保留在 PendingReplayEvents，优先排空，并把 LastPersistenceError 设置为 PersistenceOutcomeUnknown。它是已接受写入的等待记录，不是额外的新事件入口；新事件仍受原队列限额。相同EventId先去重再检查容量，重复不会因为队列满被错误拒绝。

对账失败保留 Runtime、账本和快照，停止用旧Revision写入并可重试。GetPlayerPersistenceError 返回当前真实错误；FlushPlayerProgressNow 只在在途、重放、延迟、Pending 和对账都清空后成功。成功传输不代表奖励到账。

SnapshotSequence 是服务器 StateComponent 每次发布递增的显示序列，不作为数据库 CAS Revision。客户端接纳更高 Revision；相同 Revision 仅接纳更高 SnapshotSequence。两者均为0的历史来源保留状态变化兼容，已经收到非零序列后不会被零序列同Revision覆盖。添加字段未重命名旧反射字段/协议；客户端与服务器须配套更新，否则旧客户端仍看不到同Revision进度。

涉及共享 StateComponent/Types（包括 Public/Types/GamePlatformQuestSnapshotRules.h）、Client 的 Services 与快照回归、Server 的 Subsystems 与 Private/Tests/QuestReconcileOwnershipTests.cpp。原生CMake只验证接纳和事务容量规则；UE回归使用明确测试端口，验证满16条队列、冲突、不兼容加载保留旧值、重试后全部17条事件进入真实持久化提案。测试替身不进入生产。

当前真实实现是 UE 异步端口与服务器运行机制。当前 DBAServer 文件清单未发现旧文档声称的 QuestIntegration 或 FDBAQuestPlayerDataPersistence；端口具体项目适配与 UE→后端/数据库/Outbox 联调未验收。本轮不实现或宣称这些后端能力。

## 验证与中文审核边界

本次修改已补上述责任、端侧、游戏线程、所有权、失败和取消合同；新增C++测试放在模块Private/Tests，插件Tests只持原生编译入口。原生结果与UE Automation/Editor/Client/Server编译分别记录于Game/Saved/Reviews/task3-repair-report.md，不能将纯规则通过当引擎时序通过。存量公开类型仍有逐字段中文说明缺口，本页不宣称全插件或全工作空间已经整体合规。
