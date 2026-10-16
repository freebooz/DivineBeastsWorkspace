# HUD（抬头显示基础）

`UGamePlatformHUDWidget`继承 `UCommonUserWidget（CommonUI普通用户控件）`，不自动加入 Activatable Stack（可激活栈），用于持续显示但不主导输入路由的 HUD。

Manager 通过 `AttachHUDWidget（挂载HUD）`放入 Root Layout 的 HUDLayer。HUD 的真实数据必须来自 ViewModel、Replication（复制）或 Client Model（客户端模型），不得 Tick 扫描所有 Gameplay Actor（玩法角色）。

平台 HUD 不定义 Health、Ability、Quest 等具体业务字段；这些属于上层项目 UI。

## 2026-10-11 中立资源数值显示

平台ResourceBar可选作者控件名ResourceValueText；有效只读快照格式为当前值 / 最大值，最多六位小数，无千分位分组。最大值不大于KINDA_SMALL_NUMBER、任一数值非有限或bShowValueText关闭时清空并隐藏；未绑定不伪造0/0。精确数值变化触发事件，避免微小变化跨文字格式或显示阈值后被近似比较吞掉。NativeConstruct按缓存状态恢复文字，不改字体，不引入GAS或项目依赖。平台本身不负责项目气势、头像、地图或英雄身份。

## 2026-10-15 施法条有限视觉进度

`UGamePlatformCastProgressWidget`只接收已获观察权限的`FGamePlatformUICastProgressState`事件。绑定作用域或清空时停止自有动画；接受新修订时从该事件的已过比例开始，按`RemainingSeconds`建立瞬态`UWidgetAnimation`、`UMovieScene`和原生`Percent`浮点轨道，线性插值到1。运行时对象不保存成资产，也不修改作者布局、材质或动画资产。模块直接声明`MovieScene`、`MovieSceneTracks`私有依赖。

曲线只表达已经确认的有限窗口。动画播放不读取GAS、Actor或服务器时间，不注册业务Tick、Timer或Ticker，不更改只读快照的`bActive`，也不决定技能释放、取消、冷却或资源资格。真实结束、取消、重同步与解绑仍由来源事件收敛；旧来源、旧修订不能恢复过期显示。`NativeDestruct`清空来源和自有动画，重显由上层生命周期适配器读取首次快照。

UE5.8的`SetVisibility`同步广播可见性事件；绑定、快照更新与清理均在每次可能重入的可见性/字段写入后检查本次更新身份，快照展开返回后还须复核作用域与修订，才允许创建动画。监听者同步发布后继时外层立即退出，避免重复动画失去所有权或旧清理覆盖新文字/比例；每次受理更新使用独立身份，也覆盖清空后重绑到相同Scope/Revision的情况。真实可见性重入回归为`DivineBeasts.UI.Combat.CastBarVisibilityReentry`，覆盖展开重入、Bind隐藏重入与Clear隐藏重入；尚待统一UE编译执行。

源码改动已准备，UE5.8模块链接、真实UMG播放和客户端验收尚待统一执行；静态源检查不能作为运行通过证据。
