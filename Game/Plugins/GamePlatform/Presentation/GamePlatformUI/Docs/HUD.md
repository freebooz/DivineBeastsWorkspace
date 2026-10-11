# HUD（抬头显示基础）

`UGamePlatformHUDWidget`继承 `UCommonUserWidget（CommonUI普通用户控件）`，不自动加入 Activatable Stack（可激活栈），用于持续显示但不主导输入路由的 HUD。

Manager 通过 `AttachHUDWidget（挂载HUD）`放入 Root Layout 的 HUDLayer。HUD 的真实数据必须来自 ViewModel、Replication（复制）或 Client Model（客户端模型），不得 Tick 扫描所有 Gameplay Actor（玩法角色）。

平台 HUD 不定义 Health、Ability、Quest 等具体业务字段；这些属于上层项目 UI。

## 2026-10-11 中立资源数值显示

平台ResourceBar可选作者控件名ResourceValueText；有效只读快照格式为当前值 / 最大值，最多六位小数，无千分位分组。最大值不大于KINDA_SMALL_NUMBER、任一数值非有限或bShowValueText关闭时清空并隐藏；未绑定不伪造0/0。精确数值变化触发事件，避免微小变化跨文字格式或显示阈值后被近似比较吞掉。NativeConstruct按缓存状态恢复文字，不改字体，不引入GAS或项目依赖。平台本身不负责项目气势、头像、地图或英雄身份。
