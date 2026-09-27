# GamePlatformInteraction（游戏平台交互插件）

跨游戏通用拾取、采集、场景交互和服务器合法性验证框架。正式路径为 `Game/Plugins/GamePlatform/World/GamePlatformInteraction`；本轮从旧 `World/GamePlatformInteraction`空骨架迁移同一插件身份，没有建立平行实现。

正式仅一个 `GamePlatformInteraction（交互运行模块）`，Type 为 Runtime。直接依赖 `GamePlatformCore（平台核心）`和 `GamePlatformGameplay（通用玩法）`；不直接依赖 InputClient、Character、Combat、AbilitySystem、UI、Inventory、Quest、Presentation、VFX、MOBA 或 DivineBeasts。

已实现：本地 Focus Trace、稳定 Option 选择、玩家拥有 Interactor 上的 Begin/Cancel RPC、服务器 Gameplay Active/身份/Generation/Revision/距离/LOS/并发/限流验证、Instant/Hold Session、Exclusive 与基础 Shared、请求/Commit 幂等、Target/World 生命周期取消、Door/Pickup/Harvest 当前世界状态闭环、Owner Session/Result复制与 Late Join 状态基础。

当前没有新增 Go API/微服务，不发永久 Inventory/Quest/货币奖励。UE5.8 编译、Automation 实际执行、Client/Server Cook、Multi-PIE、双客户端 Dedicated Server、网络异常和人工审查仍待真实工具链与测试地图后执行。

