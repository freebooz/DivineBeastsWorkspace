# TestingAndEvidence（测试与证据）

> 2026-09-30状态校正：本页保留旧方案/历史证据，旧DBAServer具体Quest HTTP/事件适配与后端交付宣称未在当前项目文件清单确认，不能作为现行验收。当前行为以[本轮整改说明](DesignRemediation-2026-09-30.md)、README与真实源码为准；本轮未执行数据库/Outbox/网络联调。

UE C++ Automation Test 源码当前 3 个：QuestDefinition 结构校验、QuestStateMachine 合法状态转换、QuestClient Revision 防回退。测试源码存在不等于 UE Automation 已实际运行。

`TestQuestFoundation.ps1（Quest基础静态门禁）`已实际通过：3 模块宿主、依赖边界、共享/客户端/服务器关键能力、异步 Persistence Port、单飞持久化、DeferredEvents、DBAServer Combat/Interaction/Region 适配、异步 `FHttpModule` PlayerData 传输、GameThread 回调、0 个 `.uasset/.umap`；同时明确禁止阻塞 `ProcessRequestUntilComplete（阻塞HTTP等待）`。

`TestQuestBackend.ps1（Quest后端静态门禁）`已实际通过：Go Quest领域、PlayerDataService入口、PostgreSQL Migration、Revision、EventIDs批次、Completion、Outbox同事务、`pg_advisory_xact_lock（事务咨询锁）`、Outbox Dispatcher 运行装配边界、Shared Contracts，且不存在 `Backend/cmd/QuestService`或直接 `Gold +=`。

运行型脚本已实际调用：Network 返回 `UE_ROOT/UnrealEditor-Cmd.exe unavailable`；Recovery 返回 `Go/PostgreSQL/DATABASE_URL unavailable`；Build/Cook 返回 `UE_ROOT/RunUAT.bat unavailable`。这些项目状态均为未执行。
