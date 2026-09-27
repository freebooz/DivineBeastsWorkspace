# HUD（抬头显示基础）

`UGamePlatformHUDWidget`继承 `UCommonUserWidget（CommonUI普通用户控件）`，不自动加入 Activatable Stack（可激活栈），用于持续显示但不主导输入路由的 HUD。

Manager 通过 `AttachHUDWidget（挂载HUD）`放入 Root Layout 的 HUDLayer。HUD 的真实数据必须来自 ViewModel、Replication（复制）或 Client Model（客户端模型），不得 Tick 扫描所有 Gameplay Actor（玩法角色）。

平台 HUD 不定义 Health、Ability、Quest 等具体业务字段；这些属于上层项目 UI。
