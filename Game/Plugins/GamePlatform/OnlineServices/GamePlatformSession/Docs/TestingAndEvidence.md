# Session内核测试证据（任务切换检查点）

更新日期2026-09-29。本文件只记录实际运行过的验证；任何“未执行”项不得由源码存在推断为通过。

Session纯C++状态测试通过。当前状态内核验证包含Join/Transfer/Reconnect约束、活动操作幂等、Current/Pending隔离、四事实Ready、旧操作拒绝、旅行前取消保留来源、旅行后Uncertain、远端对账、断线后Epoch防旧、旅行前TimedOut与旅行后Uncertain差异、认证代次隔离及跨Scope隔离。`VerifySession.ps1 -NativeTests` 在完整集成未完成时按设计返回2，不能把2改成0。

Backend本轮验证：`go test ./internal/app/gateway ./internal/transport/httpadapter ./internal/app/gameservercontrol` 通过；`go test -tags=grpcdeps ./internal/transport/grpcclient` 通过编译；`go test -tags=productiondeps,grpcdeps ./internal/app/composition` 通过编译。Shared OpenAPI修改后已由仓库唯一 `contractcodegen` 重新生成，并通过 `-check` 新鲜度检查。

当前仍未形成真实UE Session Transport，因此真实ClientTravel、TravelFailure、NetworkFailure、Admission Provider、两个真实客户端、Multi-PIE、断线重连、服务器Boot切换、OpenWorld/Village/MainArena跨服往返和Cook隔离均为未执行。服务端Admission子系统测试只证明边界和敏感证明生命周期，不代表Dedicated Server已经消费真实TransferTicket。

完整E2E的阻塞证据：现有GameServer注册/Transfer验证协议尚未同时提供Session所要求的 `GameSessionId / ServerBootId / ProtocolVersion / SessionEpoch`。在这些字段由可信服务器/控制面生成并绑定真实连接前，Session保持Fail Closed是验收要求，不是遗漏成功路径。
