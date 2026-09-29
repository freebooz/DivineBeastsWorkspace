# Session内核测试证据（任务切换检查点）

更新日期2026-09-29。本文件只记录实际运行过的验证；任何“未执行”项不得由源码存在推断为通过。

Session纯C++状态测试通过。当前状态内核验证包含Join/Transfer/Reconnect约束、活动操作幂等、Current/Pending隔离、四事实Ready、旧操作拒绝、旅行前取消保留来源、旅行后Uncertain、远端对账、断线后Epoch防旧、旅行前TimedOut与旅行后Uncertain差异、认证代次隔离及跨Scope隔离。`VerifySession.ps1 -NativeTests` 在完整集成未完成时按设计返回2，不能把2改成0。

Backend本轮验证：`go test ./internal/app/gateway ./internal/transport/httpadapter ./internal/app/gameservercontrol` 通过；`go test -tags=grpcdeps ./internal/transport/grpcclient` 通过编译；`go test -tags=productiondeps,grpcdeps ./internal/app/composition` 通过编译。Shared OpenAPI修改后已由仓库唯一 `contractcodegen` 重新生成，并通过 `-check` 新鲜度检查。

当前已经形成默认UE Session Transport、可靠准入RPC载体和HTTP Admission Provider实现，但“实现存在”不等于运行验证通过。真实两个客户端/Multi-PIE、跨进程ClientTravel、断线重连、服务器Boot切换、OpenWorld/Village/MainArena跨服往返和Cook隔离仍需执行。服务端Admission子系统/握手测试只证明契约、敏感证明生命周期和状态边界，不能替代真实Dedicated Server网络证据。

2026-09-29后续改造已消除原“Binding四字段缺失”阻塞：GameServer注册持有ServerBootId/ProtocolVersion，ServerTransfer签票生成GameSessionId和单调SessionEpoch，目标Server验证Boot/协议并回传完整Binding；生产Redis还原子维护已接受Epoch栅栏。当前剩余阻塞转为UE最终构建与真实E2E运行证据，而不是契约字段缺失。
