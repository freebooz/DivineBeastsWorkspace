# 世界游戏菜单与真实设备设置

2026-10-11。本说明覆盖 DBAClient 的 DivineBeastsUIClient 菜单、设备设置与唯一 ApplicationFlow 注销增量；不证明资产、构建、Cook、设备或真实网络验收已通过。

## 职责与输入生命周期

世界控制器的 `OnWorldEscapeRequested` 只广播本地意图；UI 子系统在真实控制器变化时订阅、解绑旧实例，双端 DBAWorldsRuntime 不依赖客户端 UI。`HandleEscapeAction` 只允许已认证、空闲、无加载/错误的 `DBA.Flow.InWorld` 打开游戏菜单。登录、选角、创建与传输仍由其原页面消费取消。菜单再次 ESC 或返回游戏关闭；设置内 ESC/返回先回原菜单。

菜单复用平台 System 层与 CommonUI 焦点/输入栈，目录采用 `UIOnly`、`Never` 暂停策略、不跨 Travel。入栈前调用世界控制器 `SetWorldGameplayInputSuspended(true)`，先撤销按键与鼠标拖动，再由 CommonUI 应用菜单输入；关闭层栈后恢复世界输入。UI 不调用 Pause 或 TimeDilation，不改变服务器权威。开启期间世界继续联网运行。

异步打开使用独立 ScreenId、RequestId 和本地代次；重复命令、迟到完成、取消、同步失败均不能复活旧菜单。控制器更换、平台 Travel 清页、失去应用焦点、注销和反初始化都会关闭/取消系统页，解绑旧控制器和应用事件。`OnSystemMenuVisibilityChanged` 只发布整体可见性，设置子页不会制造第二套流程。

世界输入交接也必须更新 CommonUI Router 的真实输入配置。加载页面失活后，Slate 延迟释放期间 Router 可能继续保存旧 Menu/UIOnly；只调用控制器 SetInputMode 不会更新这一状态。UIClient 在流程状态、页面真正关闭、控制器交接，以及 Router 的 `OnBoundActionsUpdated` / `OnActiveInputConfigChanged` 事件中检查：已认证、空闲、无加载/错误的 InWorld，项目主页面和系统页面无活动或在途请求，平台各层无活动页面，且 Router 无活动叶节点。全部成立才经平台 `BuildInputConfig(GameAndUI)` 与 CommonUI 公开 `SetActiveUIInputConfig` 发布 All/NoCapture。前端、菜单、外部活动页面及在途请求仍有优先权；没有逐帧轮询。

Router 的同步配置事件使用重入保护；每个本地玩家只订阅自己的 Router，替换或反初始化精确移除两份委托。退出先撤销交接资格，之后的页面关闭不能复活世界输入。UE5.8 的 Router 按新旧配置差量调整自身移动/视角 Ignore 计数；本模块不调用 ResetIgnoreMoveInput/ResetIgnoreLookInput，不清除其他组件的锁，也不替代世界控制器的暂停输入职责。

## Monolith 视觉资产合同

所有视觉必须用 Monolith MCP 创建、编译、保存并重载，代码不构造菜单视觉树。公共资产归 DBAUIPack_Core，自然藤蔓、树叶与碧绿远古风格；主按钮竖排等高，明确导航和焦点。新父类必须先完成正式 Editor 编译再生成资产，旧反射类型和已发布字段不迁移。

| 项目 | 游戏菜单 | 系统设置 |
| --- | --- | --- |
| 蓝图父类 | `/Script/DivineBeastsUIClient.DivineBeastsSystemMenuScreen` | `/Script/DivineBeastsUIClient.DivineBeastsDeviceSettingsScreen` |
| 稳定软路径 | `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_SystemMenu.WBP_DBA_UI_SystemMenu_C` | `/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Settings.WBP_DBA_UI_Settings_C` |
| 页面身份 | `UI.Screen.SystemMenu` | `UI.Screen.Settings` |
| 默认焦点 | `ResumeButton` | `ResolutionCombo` |
| 必需 UButton | `ResumeButton`、`SettingsButton`、`LogoutButton`、`QuitButton` | `ApplyButton`、`ConfirmButton`、`RevertButton`、`BackButton` |
| 必需 UComboBoxString | 无 | `ResolutionCombo`、`WindowModeCombo`、`QualityCombo` |
| 必需 UTextBlock | `ErrorText` | `StatusText`、`ErrorText` |

菜单按钮中文为返回游戏、系统设置、退出登录、退出游戏。设置按钮中文为应用、确认设置、恢复原设置、返回。资产若设置运行定义覆盖目录值，仍必须保持 System、UIOnly、Never、不跨图的安全合同。最终需检查命名控件、按钮等高、鼠标/键盘/手柄导航和焦点恢复，并维护内容包 MonolithGenerationManifest。

## 真实设置范围与所有权

`UDivineBeastsSystemSettingsViewModel` 是现有项目 ViewModel 的扩展，消费 `IGamePlatformDeviceSettingsService`。平台服务及 UGameUserSettings 唯一拥有设备设置和磁盘保存；页面只拥有订阅、未提交控件输入与一次预览，不另写 INI、SaveGame 或设置 Provider。全部操作游戏线程，低频事件更新，无 Widget Tick、HTTP 和进程级缓存。

分辨率选项来自设备支持列表并附加当前窗口尺寸；窗口模式有窗口、无边框全屏、全屏；整体画质使用 UE 0..4 的低/中/高/极高/电影，现有不同分项值显示“自定义（保留当前）”，不会默默覆盖。平台接口原有 VSync、帧率与分项画质仍保留，不因表单显示较少而覆盖。

应用先 Stage 校验，成功后 Preview，不写盘。15 秒内确认才调用平台 ConfirmPreview 保存；超时、返回、旅行、失焦或页面失活回滚本页未确认预览。暂存/应用失败显示真实中文错误，编辑器多 PIE、命令行或无设备支持环境会明确拒绝。服务同步回调期间失活延迟到命令返回后清理，防止违反平台回调重入规则。设备快照真实修订变化才更新表单，不用普通业务事件覆盖用户未提交选择。

预览期限以 `FPlatformTime::Seconds` 单调时钟计算，仅本页拥有预览期间注册临时 CoreTicker 检查截止时间；世界暂停和 TimeDilation 不延长期限。确认、回滚与页面清理精确撤销句柄，弱引用及页面代次拒绝迟到调度。它不是 Widget 业务 Tick；主线程阻塞时只能在下一次引擎调度恢复原设置。

目前 GamePlatformSFX 只有具体音效实例音量接口，没有主音量/混音持久合同；GamePlatformInput 没有完整可交付按键设置页。此轮不显示假音量或按键设置按钮。后续需先完成对应领域的真实合同与消费者迁移，再接入此菜单；不能在 UI 另存一份音频/输入真源。

## 注销与退出

退出登录调用既有项目 ViewModel.Logout、ApplicationUIAdapter、ApplicationFlow.LogoutAndRestart。此次修复注销时取消唯一流程/加载/观察代次后调用 Session.LeaveSession；平台 UE Transport 负责真实 ReturnToMainMenu 和断线，清当前可信 Binding 与准入事实。随后 Online.Logout 清认证/凭据，LoggedOut 通知触发原 StartFlow(false) 人工登录。Session 离开失败保留错误码日志，不能伪造物理断线成功；在线认证仍必须撤销。没有新增 Travel 执行器或把 Login 页面当断线。

退出游戏先清菜单，再调用引擎正常 QuitGame。此轮未自动执行退出命令、操作游戏窗口或截取游戏画面。

## 验证与剩余边界

新增 UE Automation 源码：`DivineBeasts.UI.SystemMenu.WorldRouting` 覆盖前端/忙碌/旅行/错误/注销拒绝与系统目录输入/暂停/Travel；`DivineBeasts.ApplicationFlow.Logout.SessionLifetime` 使用真实 Session 子系统、仅测试 Transport 验证传输取消、Ready 离开、重复注销和迟到事实拒绝，不代表真实联网已验。原目录测试同步撤销“设置页面未获批准”的旧断言。

`DivineBeasts.UI.Input.WorldHandoff` 使用真实 LocalPlayer、UIClient 与 CommonUI Router，验证旧 Menu 配置、加载页关闭交接、真实 Router 配置/树事件、系统及主页面在途请求阻断、前端/旅行/退出拒绝和精确解绑、跨玩家拒绝。夹具不创建视口，严格预期原生无视口警告次数，只验证真实 Router 配置状态与事件；Slate 焦点、真实捕获/Ignore 计数及联网进入世界仍需主任务运行验证。两个 `DivineBeasts.UI.SystemMenu.AssetContract` / `DivineBeasts.UI.Settings.AssetContract` 只加载真实软类并 Initialize，检查原生父类、控件类型/焦点、菜单 Canvas 固定等高与无 ScaleBox，不激活业务或修改显示设置。

本任务只执行代码/差异静态检查。正式 Editor/Client 构建、新用例运行、Monolith 资产编译保存重载、Cook、真实 ESC/导航/失焦/Travel/注销联机验证由主任务统一调度，尚未在本子任务执行。禁止将这些源码或文档写成已完成运行验收。

目录规划需由主任务同步登记：本文件；UIClient 的 `Public/Private/Screens/System/DivineBeastsSystemMenuScreen.*`、`DivineBeastsDeviceSettingsScreen.*`；`Public/Private/ViewModels/System/DivineBeastsSystemSettingsViewModel.*`；`Private/Tests/DivineBeastsSystemMenuTests.cpp`；ApplicationFlowClient 的 `Private/Tests/DivineBeastsLogoutSessionTests.cpp`；以及由 Monolith 真正生成后的两个视觉资产。

后续目录增量：UIClient `Private/Tests/DivineBeastsSystemMenuAssetTests.cpp` 与 `Private/Tests/DivineBeastsWorldInputHandoffTests.cpp`；输入交接仅增量修改既有 `Public/DivineBeastsUIClientSubsystem.h`、`Private/DivineBeastsUIClientSubsystem.cpp`，不新增模块、输入映射或世界执行器。

## 根任务实际验证补证（2026-10-11）

上述子任务未执行项由根任务统一补证：Monolith完成两份页面编译、保存、重启后重读；Editor受影响模块构建及完整Client/Server构建通过，最后Client完整链接为785a8f50-693c-4665-80ab-96999a2c4a67。Editor自动化35项无失败、1项带警告；实际发布包自动化17项全部成功，包括真实Router输入交接。最后客户端Cook/Stage为74e4e402-4383-4653-9e8e-5d8d5f1e739d，344项运行资源齐全；专服为26d44ad1-9076-46cd-b9ab-55f4ecc1cfda，760行容器清单无公共UI/前端/客户端VFX。门禁不要求独立Niagara模块源脚本进入客户端：UE5.8 `NiagaraScript.cpp::IsEditorOnly`明确将这类资源排除，运行程序消费编译后系统。

先前包测试发现单模块构建未重新链接客户端主程序；执行完整Client构建后重新发布，源程序与发布程序SHA-256一致，再次运行17项包内测试通过。失败日志不覆盖。业务五服务就绪、新专服登记并监听17778；已替换本任务拥有的两个旧窗口，首屏仍是空白人工登录。真实菜单交互、显示试用回滚、鼠标与联机注销、HUD颜色的人工验收仍待确认，没有用无视口自动化或启动记录代替。
