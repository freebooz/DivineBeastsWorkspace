# SSLM登录页插件开发入口

## 范围与决定

用户确认审查建议后，实施隔离进程方案。登录界面新增“插件开发模式”按钮，保留当前真实认证流程；点击启动已有演示门户，不在父进程追加DBAPluginDemo参数或调用OpenLevel假装完成模式切换。不创建新插件、后台服务或认证旁路。

当前GameInstance初始化时正式UI/Flow与演示目录走互斥分支，因此不提供同进程无缝切换。UI模块已有Demo模块作为消费者，不能反向依赖Demo；本次新增的私有GameInstanceSubsystem只依赖UI已有引擎模块和公共模式门禁。

## 使用与资格

从PowerShell 7执行既有入口：

```powershell
./Build/Game/LaunchDBADevelopment.ps1 -Mode Debug -Start -Target EditorGame -EngineRoot $env:UE_ROOT
```

该入口把本次PowerShell 7的实际可执行路径传给登录进程，不含账号、密码。手动启动Editor/PIE默认检查标准ProgramFiles/PowerShell/7安装；非标准安装应使用上述入口或明确的`-DBADevelopmentPowerShell`参数。不在PATH里任意搜寻同名程序。

按钮仅本地NM_Standalone开发Editor宿主可见，Shipping/Test/Commandlet/专服和打包Client不开放。当前RunPluginDemo.ps1对Client产物缺乏绑定证据，明确拒绝；本次不绕过它。游戏实例中已有检查/演示会话时按钮禁用，页面关闭不终止人工演示窗口。登录正在提交时不能启动演示；点击立即清空密码，并撤销本页开发自动登录意图。

## 生命周期与隐私

Widget提交固定意图→私有Launcher启动隐藏PowerShell桥→既有RunPluginDemo执行完整Editor输入/产物证据及资源检查→创建可见演示进程。桥使用独占RunId目录并等待自身演示进程退出；父登录窗口保持不变，可继续使用。

父进程不传递命令行、账号、密码或认证票据。桥进入后只保留操作系统运行必需环境白名单，再调用演示启动器；父进程环境不被修改。PowerShell桥本身作为OS子进程仍会最初继承环境，本方案不宣称跨进程秘密隔离沙箱。日志不主动记录环境或输入文本。

Launcher每0.5秒仅维护自己的活动进程，退出后撤销Ticker；Widget仅订阅状态事件，不Tick轮询业务。状态Preparing/Started/Exited/Failed是启动进程事实，Started不表示门户Ready、自动检查通过或人工批准。失败时UI显示固定中文说明及RunId，详细错误只在本次证据目录。不会关闭其他进程，也不会自动构建、降版本、禁用出错业务模块或删除资源。

## 验证记录与断点

- 本次工作开始HEAD=f892ffa4，保留用户未跟踪Water地图与Surface纹理。
- 脚本测试先因新入口缺失失败；补实现后`TestLoginPluginDemoLaunch.ps1`报告9项通过，包含实际缺失引擎失败与重复RunId拒绝；UEExecuted=False。
- 新增UE测试`DivineBeasts.PluginDev.LoginEntry.NoWorldFailsClosed`，必须在本次完整Editor构建后真实运行，不以测试源码存在计为通过。
- 正在执行完整Editor组合构建RunId `14e58cf2-9c28-4b83-9065-51fdc4ec7b13`，不使用OnlyModules，不修改引擎版本；最终结果另行追加。
- 本次Monolith连接初查不可用（编辑器未运行）；需构建后启动锁定编辑器、确认项目身份，再创建/编译/保存/重载真实按钮。当前不宣称蓝图已修改。
- 端到端点击、门禁失败提示、重复点击、演示窗口关闭后重试、返回原登录及Shipping/Cook仍需独立证据。

人工审查：待人工审查。审查人：＿＿；日期：＿＿。本记录不构成发布批准。
