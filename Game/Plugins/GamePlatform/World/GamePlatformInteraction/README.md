# GamePlatformInteraction（游戏平台交互插件）

跨游戏通用拾取、采集、机关/NPC 等世界交互与服务器合法性验证框架。正式路径固定为 `Game/Plugins/GamePlatform/World/GamePlatformInteraction`，属于 `GamePlatform（平台层）/World（世界能力分类）`；不建立第二套交互框架，也不把项目生肖、MOBA 竞技或界面表现规则下沉到本插件。

正式仅一个 `GamePlatformInteraction（交互运行模块）`，Type 为 Runtime（共享运行时模块）。公开构建依赖仅保留公开类型真正使用的 Engine、GameplayTags、DeveloperSettings 等引擎模块；`GamePlatformGameplay（通用玩法）`只作为 Private（私有实现）依赖读取服务器玩法资格 Provider（提供者）。不直接依赖 InputClient、Character、Combat、AbilitySystem、UI、Inventory、Quest、Presentation、VFX、MobaCommon 或 DivineBeasts。

已实现：本地 Focus Trace（焦点射线）、同一命中 Actor（实体）上的多 InteractableComponent（可交互组件）稳定候选选择、玩家拥有 Interactor（交互发起器）上的 Begin/Cancel RPC（开始/取消远程调用）、服务器 Gameplay Active（玩法活跃资格）/组件实例身份/Generation（代次）/Begin Revision（开始时修订）/Option（选项）/距离/LOS（视线）/并发/独立限流窗口验证、Instant/Hold（即时/长按）会话、Exclusive/Shared（独占/共享）并发、请求与 Commit（提交）幂等、弱会话清理、Target/World（目标/世界）生命周期取消、Door/Pickup/Harvest（门/拾取/采集）当前世界状态闭环、Owner Only（仅拥有者）Session/Result（会话/结果）复制与 Late Join（晚加入）状态基础。

当前没有新增 Go API（Go 接口）/微服务，不发永久 Inventory（背包）/Quest（任务）/货币奖励。源码层采用无 Tick（每帧轮询）设计，Focus 与 Hold 使用可配置 Timer（定时器）；服务器始终重验权威事实。构建、Automation（自动化测试）、Dedicated Server（专用服务器）、Multi-PIE（多实例编辑器运行）和网络异常验证的真实执行证据见 `Docs/TestingAndEvidence.md（测试与证据）`；未执行项目不得在文档中冒充完成。

