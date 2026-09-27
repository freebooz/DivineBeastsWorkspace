# ClientPanel（客户端调试面板）

GamePlatformDebugClient（客户端调试模块）提供独立 Slate Overlay（Slate叠加面板），不修改正式 HUD（抬头显示界面）。

功能：
- Category Tabs（分类标签）：World/Character/Ability/Combat/AI/Navigation/Network/Online/Session/Loading/Telemetry。
- Target Selector（目标选择）：LocalPawn（本地角色）/ViewTarget（视图目标）。
- Search（搜索字段）。
- Refresh Rate（刷新频率）：Manual、1Hz、5Hz、10Hz（手动/每秒1/5/10次）。
- Pause（暂停）。
- Copy Sanitized Summary（复制脱敏摘要）。
- Overlay Draw（叠加绘制）：选中目标 Bounds（包围盒）。

面板默认 1Hz。面板关闭时移除 FTSTicker（核心Ticker），停止 Provider 高频采集。不会以 60Hz 做全量状态采集。

客户端世界的 Server View（服务器视角）不会把本地值伪装成服务器值；客户端被明确引导到 Gameplay Debugger（玩法调试器）查看权威复制分类。