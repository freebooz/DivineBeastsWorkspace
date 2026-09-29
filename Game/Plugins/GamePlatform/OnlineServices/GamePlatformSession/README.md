# GamePlatformSession（平台会话与跨服插件）

当前交付状态：**客户端会话公共子系统、状态/恢复内核、Gateway世界进入入口和服务器准入边界已具备；真实UE网络Transport仍保持Fail Closed（失败关闭），完整跨服E2E尚未完成。** 现有实现不再是“只有私有状态内核”，但也不能据此宣称真实ClientTravel和Dedicated Server准入已经闭环。

面向玩家的目标是：Online认证后由Gateway校验当前选中角色并请求GameServerControl分配世界与一次性TransferTicket，再由Session执行加入、迁移、重连、离开和恢复。当前 `UGamePlatformSessionClientSubsystem` 已经公开Intent、Recovery、Cancel/Leave/Disconnect/Reconcile等通用能力，并保持TransferTicket不进入公开Snapshot；GamePlatformServer侧已经建立只接受可信C++握手层提交证明的Admission边界。

模块仍只有GamePlatformSession，允许Client/Editor、禁止Server。Session平台层不依赖DivineBeasts、MobaCommon、ApplicationFlow、UI或Telemetry；项目层DBAClient单向依赖Session。服务器准入由独立的GamePlatformServer负责，Session不会把服务器职责重新拉回客户端插件。

当前Backend已经补齐受认证的 `POST /v1/divinebeasts/world-entry` 公共入口，并将GameServerControl内部HTTP控制面统一置于内部Bearer保护下；HTTP/gRPC Gateway装配均复用现有GameServerControl能力，没有新增第六个Session微服务。仍未完成的关键前置是：控制面/服务器握手尚未向客户端Session提供可信 `GameSessionId / ServerBootId / ProtocolVersion / SessionEpoch` 绑定，因此禁止用客户端自造值实现ClientTravel成功。

建议人工先读交付状态和安全边界，再查API/状态机及测试证据。12类说明对应如下：

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

源码存在、文档齐全、状态测试或Gateway/Backend测试通过，均不构成完整Session网络链交付完成。只有在可信Binding、真实ClientTravel、Server Admission、NetworkFailure/TravelFailure、重连及OpenWorld/Village/MainArena端到端验证全部通过后，才能提升为生产完成状态。
