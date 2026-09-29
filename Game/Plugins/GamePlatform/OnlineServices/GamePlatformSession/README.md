# GamePlatformSession（平台会话与跨服插件）

当前交付状态：**客户端会话公共子系统、状态/恢复内核、Gateway世界进入、可信Binding、默认UE Transport（传输适配器）、服务器Admission Provider（准入提供者）与可靠RPC握手均已实现；完整跨服E2E仍待最终构建和多进程验证。** 当前不能仅凭源码存在宣称OpenWorld/Village/MainArena真实跨服已经完成验收。

面向玩家的目标是：Online认证后由Gateway校验当前选中角色并请求GameServerControl分配世界与一次性TransferTicket，再由Session执行加入、迁移、重连、离开和恢复。当前 `UGamePlatformSessionClientSubsystem` 已经公开Intent、Recovery、Cancel/Leave/Disconnect/Reconcile等通用能力，并保持TransferTicket不进入公开Snapshot；GamePlatformServer侧已经建立只接受可信C++握手层提交证明的Admission边界。

模块仍只有GamePlatformSession，允许Client/Editor、禁止Server。Session平台层不依赖DivineBeasts、MobaCommon、ApplicationFlow、UI或Telemetry；项目层DBAClient单向依赖Session。服务器准入由独立的GamePlatformServer负责，Session不会把服务器职责重新拉回客户端插件。

当前Backend已经补齐受认证的 `POST /v1/divinebeasts/world-entry` 公共入口，并将GameServerControl内部HTTP控制面统一置于内部Bearer保护下；HTTP/gRPC Gateway装配均复用现有GameServerControl能力，没有新增第六个Session微服务。TransferTicket由控制面绑定 `GameSessionId / ServerBootId / ProtocolVersion / SessionEpoch`，客户端只消费公开ExpectedBinding；目标Dedicated Server通过Admission Provider再次验票并逐字段确认Binding。服务端还维护“已接受SessionEpoch”原子栅栏，更高Epoch准入后旧票不能重新进入。

建议人工先读交付状态、安全边界和最佳执行计划，再查API/状态机及测试证据。13类说明对应如下：

- D01：[本README](README.md)，总体入口。
- D02：[Architecture.md](Docs/Architecture.md)，实际目录、职责和依赖。
- D03：[API.md](Docs/API.md)，真实内部签名、调用顺序与公开服务缺口。
- D04：[StateMachine.md](Docs/StateMachine.md)，实际状态、取消和代次规则。
- D05：[BackendIntegration.md](Docs/BackendIntegration.md)，真实数据库方法、契约及未接线边界。
- D06：[Security.md](Docs/Security.md)，身份、重放、租约及两条网络边界。
- D07：[ConfigurationAndRun.md](Docs/ConfigurationAndRun.md)，参数、工具及可复现命令。
- D08：[TestingAndEvidence.md](Docs/TestingAndEvidence.md)，SESS-01—18与真实证据。
- D09：[Troubleshooting.md](Docs/Troubleshooting.md)，症状和安全处理。
- D10：[MigrationAndHandover.md](Docs/MigrationAndHandover.md)，迁移/回退前提和后续接入。
- D11：[ManualReview.md](Docs/ManualReview.md)，待人工填写的审查清单。
- D12：[DeliveryStatus.md](Docs/DeliveryStatus.md)，完成、未完成与续作断点。
- D13：[最佳修改方案与执行计划.md](Docs/最佳修改方案与执行计划.md)，本轮审查后的目标架构、P0/P1/P2实施顺序、已完成项、阻塞项与验收清单。

源码存在、文档齐全、状态测试或Gateway/Backend测试通过，均不构成完整Session网络链交付完成。只有在可信Binding、真实ClientTravel、Server Admission、NetworkFailure/TravelFailure、重连及OpenWorld/Village/MainArena端到端验证全部通过后，才能提升为生产完成状态。
