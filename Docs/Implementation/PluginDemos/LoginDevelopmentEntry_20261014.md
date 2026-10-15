# SSLM登录页插件开发入口

## 范围与决定

用户确认审查建议后，实施隔离进程方案。登录界面新增“插件开发模式”按钮，保留当前真实认证流程；点击启动已有演示门户，不在父进程追加DBAPluginDemo参数或调用OpenLevel假装完成模式切换。不创建新插件、后台服务或认证旁路。

当前GameInstance初始化时正式UI/Flow与演示目录走互斥分支，因此不提供同进程无缝切换。UI模块已有Demo模块作为消费者，不能反向依赖Demo；本次新增的私有GameInstanceSubsystem只依赖UI已有引擎模块和公共模式门禁。

## 使用与资格

从PowerShell 7执行既有入口：

```powershell
./Build/Game/LaunchDBADevelopment.ps1 -Mode Debug -Start -Target EditorGame -EngineRoot $env:UE_ROOT -BuildResultPath $EditorBuildResult
```

`$EditorBuildResult`必须是当前源码完整Editor构建成功后的`Saved/Validation/FoundationM0/<RunId>/Build/result.json`绝对路径，失败记录不能用于启动。该入口将此路径与本次PowerShell 7的实际可执行路径传给登录进程，不含账号、密码。不自动寻找“最新”旧证据；缺失证据时按钮给出明确提示，不启动子进程。手动启动Editor/PIE需显式提供`-DBADevelopmentBuildResult`，默认检查标准ProgramFiles/PowerShell/7安装；非标准安装需提供`-DBADevelopmentPowerShell`。不在PATH里任意搜寻同名程序。

按钮仅本地NM_Standalone开发Editor宿主可见，Shipping/Test/Commandlet/专服和打包Client不开放。当前RunPluginDemo.ps1对Client产物缺乏绑定证据，明确拒绝；本次不绕过它。游戏实例中已有检查/演示会话时按钮禁用，页面关闭不终止人工演示窗口。登录正在提交时不能启动演示；点击立即清空密码，并撤销本页开发自动登录意图。

## 生命周期与隐私

Widget提交固定意图→私有Launcher启动隐藏PowerShell桥→既有RunPluginDemo执行完整Editor输入/产物证据及资源检查→创建可见演示进程。桥使用独占RunId目录并等待自身演示进程退出；父登录窗口保持不变，可继续使用。

父进程不传递命令行、账号、密码或认证票据。桥进入后只保留操作系统运行必需环境白名单，再调用演示启动器；父进程环境不被修改。PowerShell桥本身作为OS子进程仍会最初继承环境，本方案不宣称跨进程秘密隔离沙箱。日志不主动记录环境或输入文本。

Launcher每0.5秒仅维护自己的活动进程，退出后撤销Ticker；Widget仅订阅状态事件，不Tick轮询业务。状态Preparing/Started/Exited/Failed是启动进程事实，Started不表示门户Ready、自动检查通过或人工批准。失败时UI显示固定中文说明及RunId，详细错误只在本次证据目录。不会关闭其他进程，也不会自动构建、降版本、禁用出错业务模块或删除资源。

## 验证记录与断点

- 本次工作开始HEAD=f892ffa4，保留用户未跟踪Water地图与Surface纹理。
- 脚本测试先因新入口缺失失败；补实现后`TestLoginPluginDemoLaunch.ps1`报告9项通过，包含实际缺失引擎失败与重复RunId拒绝；UEExecuted=False。
- 新增UE测试`DivineBeasts.PluginDev.LoginEntry.NoWorldFailsClosed`，必须在本次完整Editor构建后真实运行，不以测试源码存在计为通过。
- 完整Editor组合构建RunId `14e58cf2-9c28-4b83-9065-51fdc4ec7b13`失败，Win64 Development/UE5.8.0，退出码6，未使用OnlyModules。`DivineBeastsUIColorPipelineTests.cpp:225`的局部Pages隐藏`DivineBeastsPartyUIClientSubsystem.cpp:16`的全局Pages（C4459）。本次新增Launcher与测试编译动作成功不等于完整构建通过；未关闭警告或修改该无关测试。构建期间追加的证据参数修正尚未编译，不能使用本次失败记录启动演示。
- 补齐BuildResultPath传递后，脚本回归10项通过（UEExecuted=False）；仍需真实UE端到端验收。
- 已启动锁定UE5.8编辑器，Monolith 0.23.0确认项目绝对路径。真实登录资产通过Monolith新增按钮、标签和状态文本，树由20增至23项；父类不变。按钮位于右上角，默认Collapsed；样式为无新增纹理的绿色圆角按钮，正常/悬停/按下/禁用四态，标签和状态字号18。蓝图编译0错误、0警告，明确保存1个包；没有保存其他资产。新模块未成功链接，不能将本次旧模块下资产编译记为C++验收通过。
- 静态焦点审计无悬空显式目标；因父类不暴露该工具所识别的默认焦点字段，工具未计算可达性，不能据此称键盘验证通过。按钮Next/Down指向账号、Previous指向登录；新C++在按钮可用时插入Tab环，禁用时恢复原环，这段新增代码未编译运行。可访问性初检10项字号提示，其中新状态文本已由14调至18，其余9项属于原有控件，本次未扩改。
- 未调用已知错误的widget截图路径，未新导入或重导入纹理；布局、颜色、窄窗口和手柄人工验收仍待执行。关闭本次编辑器PID32104后，在新编辑器PID20692中核对同一项目绝对路径并重新读取资产：23控件、父类不变、新按钮/状态默认Collapsed、导航目标与18号字体保留，磁盘文件113755字节，目标包无未保存修改。复查可访问性0错误、9项原有字号警告。冷重载通过不等于按钮运行验收通过。
- 端到端点击、门禁失败提示、重复点击、演示窗口关闭后重试、返回原登录及Shipping/Cook仍需独立证据。

人工审查：待人工审查。审查人：＿＿；日期：＿＿。本记录不构成发布批准。

中文说明检查范围：新增启动桥、私有Launcher、登录绑定/解绑/导航、新回归测试及开发启动器参数。已补齐职责、参数、状态、失败、凭据、资源释放与测试边界说明；未进行全项目存量注释合规审计。工作期间检测到外部提交推进HEAD，未撤销、覆盖或代替用户提交。
