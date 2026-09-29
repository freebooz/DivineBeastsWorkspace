# Architecture（架构）

`GamePlatformInteraction（游戏平台交互插件）`正式位于 `Game/Plugins/GamePlatform/World/GamePlatformInteraction`，属于三层架构最底层 `GamePlatform（平台层）` 的 `World（世界能力分类）`。插件只保留一个 `GamePlatformInteraction（交互运行模块）`，Type 为 Runtime（共享运行时）。客户端候选发现和服务器权威验证共用中立数据契约，服务端逻辑通过 Authority（服务器权威）/RPC（远程过程调用）路径执行。

模块公开边界不导出任何其他 GamePlatform 插件类型；`GamePlatformGameplay（通用玩法）`仅作为 Private（私有实现）依赖，用于读取 `IGamePlatformGameplayEligibilityProvider（玩法资格提供者接口）`。已移除未使用的 `GamePlatformCore（平台核心）`依赖。没有直接依赖 InputClient、Character、Combat、AbilitySystem、UI、Inventory、Quest、Presentation、VFX、MobaCommon 或 DivineBeasts，因此满足 DivineBeasts → MobaCommon → GamePlatform 的单向三层依赖约束。

主要链路：本地 Focus Trace（焦点射线）→ 同一命中 Actor 上按组件实例与 Option（交互选项）稳定选择 → FocusSnapshot（焦点快照）→ 玩家拥有的 InteractorComponent（交互发起组件）发送 Begin RPC（开始远程请求）→ 服务器按 TargetInstanceId（目标实例ID）精确解析组件并重验 Gameplay Active/Generation/Begin Revision/Option/距离/视线/并发/限流 → Session（会话）→ Instant/Hold（即时/长按）→ Interactable Commit（目标提交）→ 复制当前世界状态。

`TargetRevision（目标修订号）`是 Begin（开始）阶段的乐观并发令牌：客户端观察到的 Revision 必须与服务器当前值一致；会话成功占用后，不再把每次正常 Commit 产生的 Revision 变化当作会话租约失效条件。配置、可用性、代次等真正会让现有会话失效的变化，由服务器 Authority API（权威接口）显式调用 CancelActiveSessions（取消活动会话）。`TargetGeneration（目标代次）`则在整个会话期间始终严格校验。
