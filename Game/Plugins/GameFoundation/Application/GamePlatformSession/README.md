# GamePlatformSession（平台会话与跨服插件）

当前交付状态：**独立状态/数据库内核已实施，完整会话插件未完成，真实UE与Go联调被前置阻塞。** 本轮发现Online目录没有任何源码或公开接口，Foundation测试地图也不存在。不能把前四插件提示词当作真实前置实现。

面向玩家的目标是：登录后进入后端分配的真实游戏服务器，完成服务器准入和绑定确认，再支持迁移、重连与离开。当前已写的是其中的状态安全规则和后端原子准入事务；没有自动连接、公开会话服务、UI或真实游戏握手。离开游戏应保留Online认证，退出账号应使旧操作失效；当前内核只处理非敏感认证身份和代次，不持有令牌。

模块只有GamePlatformSession，允许Client/Editor、禁止Server，默认不启用。当前实际依赖只有UE Core用于模块注册；未导入不存在的Online接口，也未复制认证、资源加载或ApplicationFlow。前三插件的原生回归单独记录，尚未用于真实会话装配。

完整成功路径仍待接通：Online真实认证 → 可信玩家/角色授权 → 真实服务器注册就绪 → 预留及握手 → 服务器领取/提交 → 客户端四事实Ready。当前可以执行原生状态测试和独占PostgreSQL测试；它们不代表玩家已经进入三维场景。

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

源码存在、文档齐全、原生测试或数据库测试通过，均不构成完整Session服务交付完成。当前禁止用于公开网络或生产发布。
