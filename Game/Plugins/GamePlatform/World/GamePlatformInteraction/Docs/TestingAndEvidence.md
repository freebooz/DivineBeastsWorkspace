# TestingAndEvidence（测试与证据）

2026-10-31 新增`Private/Tests/InteractionClientCommitBridgeTests.cpp`，四个原生用例统一前缀`GamePlatform.Interaction.Network.ClientCommit`：`Snapshot`验证成功快照独立于当前会话、重复及本地权威回声；`RejectsInvalidIdentity`验证完整身份、接收者、错World及已销毁目标；`RejectsStaleScope`覆盖Pawn和Controller宿主的拥有关系换代、失活/重显及EndPlay；`ReentrantDelivery`覆盖真实动态监听中的重复消息/失活和有界历史淘汰。测试直接调用生产RPC接收实现与发送记录边界，不替代真实网络可靠性测试。

本次先写回归后修改机制，但按主任务分工未执行Editor构建、RED/GREEN或Automation；不可记录为测试通过。后续由主任务统一Editor构建和精确运行以上四项，并保留既有`GamePlatform.Interaction.Lifecycle.TerminalListenerStartsDifferentTarget`重入回归。新的Client RPC需要同步重建客户端与服务器，并另证实际联机传输、延迟/断线及Cook。

以下为历史复审记录，不代表2026-10-31新增链路已验收。既有C++ Automation包含InteractionOption结构验证、Focus稳定排序/距离过滤、组件与Request身份构造，以及后续生命周期/同步重入回归。

`Build/Validation/VerifyInteraction.ps1（交互插件静态工程门禁）`已于本轮复审执行通过，确认平台层/World 归属、单 Runtime（共享运行时）模块、公开依赖最小化、无上层直接依赖、无 Tick（每帧轮询）、仅两个低频 Server RPC（服务器远程调用）、组件实例身份、请求/提交幂等、失效弱会话清理、Hold（长按）上限、Begin/Cancel（开始/取消）独立限流、TargetGeneration（目标代次）安全回绕，以及 Custom（自定义）提交默认 Fail-Closed（失败关闭）。

`Tests/Architecture/ValidateInheritanceBoundaries.ps1（三层继承/Public API 边界门禁）`本轮执行通过：PublicHeaders=389、Types=762、Edges=107。`Tests/Architecture/ValidateProjectHeaders.ps1（项目自有头文件预检）`执行通过：检查 409 处自有头文件引用，缺失 0。`Tests/Architecture/ValidateDesignBaseline.ps1（全工作区设计基线门禁）`本轮执行未通过，共 22 项；其中 12 项为 DBAHeroPack_*（十二生肖内容包）缺少真实 UE 资产，10 项为 DBAWorldsRuntime（项目世界运行模块）对 GamePlatformCore/GamePlatformData（平台核心/平台数据）跨插件依赖未在插件描述中声明。当前失败项均不位于 GamePlatformInteraction，本插件三层继承与公开 API 源码边界已通过，但全工作区基线仍被其他模块阻塞。

UE Automation（自动化测试）、Multi-PIE（多实例编辑器运行）、Dedicated Server（专用服务器）双客户端、Late Join（晚加入）、网络延迟/断线和 Client/Server Cook（客户端/服务器烘焙）本轮尚未形成新的完成证据。Runner（运行器）已通过绝对路径 `F:\UnrealEngine-5.8.0-release` 成功启动 UnrealBuildTool（虚幻构建工具）：一次 DivineBeastsArenaEditor 全量构建确认进入真实编译并推进到 185/3287 个动作后，为避免把插件验证扩展为整套源码引擎冷构建而由本会话主动停止；随后按 UE5.8 UBT 源码已确认支持的 `-Module=GamePlatformInteraction` 发起模块级 Editor Target 构建，但同工作区另一会话的 `DivineBeastsArenaEditor` UBT 持有全局构建互斥锁，本会话的模块构建未进入编译并已主动停止。因此本轮不把并发中的外部构建或等待状态冒充为 UE 编译通过证据。
