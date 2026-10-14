# SSLM血条、冷却遮罩与事件通知（2026-10-14）

本次按用户要求新增其他玩家和怪物的头顶生命条，V键及HUD按钮切换显示；技能图标保持可见，半透明遮罩从12点方向顺时针消退。鼠标沿用既有左键环绕、右键转向和滚轮距离控制，新增最终镜头变化通知用于世界UI重投影。法术不消耗气势，仍接受原生GAS冷却和权威资格检查。

## 实装范围与所有权

- 平台GamePlatformUI提供只读世界UI资源请求、名称板数值投影、视口通知和作用域Root就绪事件。原30Hz投影Ticker已移除；平台不知道项目角色和怪物。
- DBAClient的WorldHealthBarPresenter按LocalPlayer／World持有观察，初始化盘点一次已有角色，此后消费Spawn、EndPlay、Destroyed、组件位置、ASC生命及上限、怪物复制LifeState事件。自身已有主HUD，头顶条仅显示其他实体；未知上限隐藏，不伪装满血。最大128项，显示距离6000厘米，头顶偏移24厘米。
- DBAWorlds控制器在引擎完成镜头更新后比较实际最终位置、旋转和FOV，变化才通知；不能以鼠标输入猜测最终镜头。V键只在当前世界交互且未被UI阻断时生效。
- 平台槽位的事件入口设置每实例MID的截止时刻和总时长，渲染材质使用Slate实时时钟播放；Widget不Tick读业务状态。清除动画不放宽技能资格、不补气势。
- Root初始化先于UIManager安装服务时，Presenter在Root就绪后重发已有缓存；Root替换将同一请求迁入新层，强引用与代次检查防止同步重入复活旧Widget。换账号、换世界、Possess、EndPlay及销毁成对解绑。

## 真实资产

均使用正式Game/DivineBeastsArena.uproject、UE5.8和Monolith0.23.0创建或修改并编译保存，未改地图、PlayerStart或源图RGB。

| 资产 | 实际变化 |
| --- | --- |
| /DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot | 新增CooldownSweep Image，旧CooldownOverlay收起，原图标与边框保留。 |
| /DBAUIPack_Core/UI/Materials/Combat/M_DBA_CooldownSweep | 复用平台RadialCooldown，旋转UV确定12点起始，50%半透明颜色，不复制另一套平台机制。 |
| /DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar | 160×44固定名称板，NameText、ResourceFill和ValueText；投影时首次预排版并按DPI居中。 |
| /DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD | 添加血条按钮、V键说明、工具提示、18号字及明确导航边界；Monolith可访问性检查0问题。 |
| /DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout | WorldHealthBarClass指向真实名称板类，提供Cook硬引用及运行装配。 |

新编辑器39384冷重载确认上述类引用、名称板和技能槽控件树，关闭前dirty_packages=0；证据为Saved/Validation/Rebuild20261014/d2d4518f-080b-47ca-8c39-c2d608646485/WorldUIAssets.ColdReload.json。不使用已知Gamma错误的Monolith widget截图路径。

## 检查与验收边界

原生回归验证资源变化、未知上限、静止首投影、隐藏后重显、冷却事件参数与清除、Possess／EndPlay／解绑、真实服务Root就绪及替换。鼠标控制用例验证现有输入行为。完整结果、构建、真实RHI颜色、Cook和最终运行身份集中记录在同目录FinalValidation.json，失败历史报告保留；不得仅凭UE进程退出0宣称全部测试成功。

初次UI回归的资源尺寸失败来自夹具未持有TakeWidget返回的Slate表面，已按真实Overlay所有权修正；后续30项回归的生命周期失败来自夹具未初始化World控制器列表，已按引擎真实初始化流程补齐，并增加控制器解析和Possess前置断言。重要源码问题的独立复审包括Root首次安装、同步重入、SameWorld Possess与EndPlay，不以隔离测试代替联机、实际流送和人工画面验收。

## 全局策略与历史遗留登记

用户要求已写入C:/Users/Administrator/.codex/AGENTS.md的“用户界面事件通知全局策略（2026-10-14）”，并同步工作空间AGENTS.md。首次绑定允许读取完整快照，后续状态更新使用通知；连续视觉交给引擎动画／材质，生命周期清理与代次检查必须成对实现。

本轮检索覆盖DBAClient/DivineBeastsUIClient、DBAArena/DivineBeastsArenaClient、GamePlatformUI源码，发现下列存量项，未将本次事件驱动修复宣称为全部旧UI迁移：

| 现有路径 | 现状与后续处理 |
| --- | --- |
| DBAArena/Private/UI/HUD/DivineBeastsArenaHUD.cpp | PhaseClock每0.2秒读取阶段剩余时间，需迁移为阶段事件设置截止值和引擎展示。 |
| DBAClient/Private/Social/DivineBeastsPartyScreenBinding.cpp | DeadlineTimer每0.25秒更新请求倒计时，需随组队观察合同事件接入时迁移。 |
| DBAClient/Private/ViewModels/System/DivineBeastsSystemSettingsViewModel.cpp | PreviewTimeout常驻0.1秒Ticker，需改为设置服务的预览结束／撤销通知，核对显示倒计时。 |
| DBAClient/Private/Development/DivineBeastsPluginDemoLauncher.cpp | 仅开发启动器每0.5秒检测外部进程，需与生产业务轮询区分并改为退出通知。 |
| GamePlatformUI/Private/Feedback/GamePlatformFloatingTextWidget.cpp | NativeTick视觉漂浮动画需核对是否仅引擎动画职责，不能据Tick字样断言业务轮询。 |

Notification／Feedback的一次性寿命定时器与UIManager的下一帧输入恢复属于事件调度，检索命中不自动等同状态轮询。未全量审计Blueprint绑定、其他插件或所有历史UI。后续新建、修改链路必须执行全局策略，不继续新增轮询。

中文说明检查覆盖新增／修改的请求、槽位、投影服务、Presenter、Root、控制器、材质作者工具和真实回归夹具；第三方与生成源码未手改。用户人工登录、联机伤害、实际鼠标画面及视觉验收仍由实际运行确认。
