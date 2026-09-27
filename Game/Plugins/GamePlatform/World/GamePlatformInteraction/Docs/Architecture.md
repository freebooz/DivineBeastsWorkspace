# Architecture（架构）

`GamePlatformInteraction（游戏平台交互插件）`正式位于 `GameFoundation/Gameplay`，只保留一个 `GamePlatformInteraction（交互运行模块）`，Type 为 Runtime（运行时）。客户端候选发现和服务器权威验证共用中立契约，服务端逻辑通过 Authority/RPC 路径执行。

直接项目依赖仅为 `GamePlatformCore（平台核心）`和 `GamePlatformGameplay（通用玩法）`。没有直接依赖 InputClient、Character、Combat、AbilitySystem、UI、Inventory、Quest、Presentation、VFX、MobaCommon 或 DivineBeasts。

主要链路：本地 Focus Trace（焦点射线）→ FocusSnapshot（焦点快照）→ 玩家拥有的 InteractorComponent 发 Begin RPC → 服务器 Gameplay Active/Target身份/Generation/Revision/Option/距离/视线/并发/限流验证 → Session → Instant/Hold → Interactable Commit → 复制当前世界状态。

原空骨架位于 `GameFoundation/World/GamePlatformInteraction`，本轮迁移同一插件身份到 Gameplay；没有建立第二套 Interaction 框架。
