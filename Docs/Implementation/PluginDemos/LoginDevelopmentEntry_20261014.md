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

按钮仅本地NM_Standalone开发宿主可见，Shipping/Test/Commandlet/专服始终不开放。打包Development Client自2026-10-15起可由`RunFrontEndClients.ps1 -DevelopmentTools -DevelopmentBuildResult <当前完整Editor结果> -EngineRoot <锁定引擎根> -GatewayUrl <明确网关>`显式开放。启动器传递固定源码工作空间、真实引擎根、完整Editor结果及当前PowerShell 7路径；Cook包不得把自身Stage目录当作源码或编辑器根。入口核对项目名、源码根名、固定uproject与固定桥接脚本，Editor仍核对当前项目路径。

点击后始终启动独立EditorGame门户；当前RunPluginDemo.ps1对Client演示产物缺乏绑定证据仍明确拒绝，不用当前普通Client的Cook结果冒充全部演示资源已包含。完整Editor构建的源码/产物绑定、资源存在性与来源快照仍由既有唯一门禁重新执行，不自动构建。游戏实例中已有检查/演示会话时按钮禁用，页面关闭不终止人工演示窗口。登录正在提交时不能启动演示；点击立即清空密码，并撤销本页开发自动登录意图。

`-DevelopmentTestAccounts`为独立显式开关：前端启动脚本从当前父进程的`DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON`读取既有种子，完整验证恰好十个唯一账号、非空口令与字段形状后，才把规范化JSON置入本次子进程专用`DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON`；客户端命令行只有无秘密开关，不设置自动登录。普通启动删除全部旧/新开发凭据变量；任何启动证据最多记录开关及账号名，不记录JSON、口令或令牌。既有本地Identity的账号可通过受控只读检查获取，禁止把生产账号或另一个网关的资料当成本地种子。账号准备脚本`PrepareOnlineTestData.ps1`负责每轮独立A/B账号，不能拿它覆盖现有十账号。

## 生命周期与隐私

Widget提交固定意图→私有Launcher启动隐藏PowerShell桥→既有RunPluginDemo执行完整Editor输入/产物证据及资源检查→创建可见演示进程。桥使用独占RunId目录并等待自身演示进程退出；父登录窗口保持不变，可继续使用。

父进程不传递命令行、账号、密码或认证票据。桥进入后只保留操作系统运行必需环境白名单，再调用演示启动器；父进程环境不被修改。PowerShell桥本身作为OS子进程仍会最初继承环境，本方案不宣称跨进程秘密隔离沙箱。日志不主动记录环境或输入文本。

Launcher采用一次后台线程阻塞等待本次进程退出，再回到游戏线程发布完成通知；Widget、Launcher均无Tick、Ticker或定时器轮询。启动后显示固定检查/独立窗口说明，完成时只读一次桥接终态文件。弱引用、LaunchId与生命周期代次共同拒绝失活或新请求之后的旧完成；GameInstance失活仅撤销显示许可，等待线程最终关闭其自身句柄，不终止演示进程。状态Preparing/Started/Exited/Failed仍是启动进程事实，不表示门户Ready、自动检查通过或人工批准。失败时UI显示固定中文说明及RunId，详细错误只在本次证据目录。不会关闭其他进程，也不会自动构建、降版本、禁用出错业务模块或删除资源。

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

## 2026-10-15开发Client入口增量

修复范围为Launcher的显式开发Client宿主许可、源码/引擎路径传递、一次后台等待与失活隔离，以及前端启动器的十账号瞬态环境合同。`DivineBeasts.PluginDev.LoginEntry.LaunchLifetime`覆盖正常开始/结束、重复点击、错误身份、旧代次、失活与新生命周期；`NoWorldFailsClosed`继续覆盖无世界拒绝。UE用例必须在本轮统一构建后实际执行，新增源码不计为通过。

无UE脚本回归实际调用十账号校验和ProcessStartInfo环境清理函数，覆盖正常、默认关闭、缺失/畸形JSON、数量不足、重复账号、空口令及普通/开发子进程隔离；另检查显式参数合同、演示入口凭据清理、真实缺失引擎失败与重复RunId拒绝。这些回归不启动UE，不替代Monolith保存/冷重载、客户端点击、完整Editor门禁、Cook或人工验收。

本次实际证据：`Saved/Validation/PluginDemos/LoginEntry20261015-59ddfe2b-2b80-43ab-846c-e597de58659a/Result.json`。登录启动合同29项、演示运行器合同14项、双模式源码13项及显式网关合同均退出0；缺失引擎和重复RunId的非零子进程属于预期失败路径。最初WindowsApps的python别名未能启动，不能沿用旧`LASTEXITCODE`记为通过；结果文件已明确登记初次启动失败，并以捆绑Python独立重试的真实13项结果为准。UE/Build均未执行。

### 十账号真实登录控件

现有本地身份服务已配置 `player01` 至 `player10` 十个账号及共同默认口令，本轮没有重建身份/玩家资料容器或覆盖账号。开发启动器显式选取该环境种子后，仅向本次客户端提供专用进程环境。登录页激活只保存十个可显示名称；点击时重新核对完整配置、索引和名称，瞬时将口令交给既有ViewModel登录命令，不填入密码框、不进入视图状态、资产默认值、日志或遥测。忙碌、失败、维护及重复点击继续使用真实认证状态与命令完成事件；失活清理名称、敏感输入和十组点击订阅，激活代次拒绝旧同步调用栈覆盖新页面。

Monolith在正式UE5.8进程27104中原位修改并保存 `WBP_DBA_UI_Login`，23→45个控件；新增容器、标题及十组按钮/标签，按钮108×44、字体18，原账号密码框尺寸字号保持。插件入口文字为“进入插件化开发模式”。十按钮补齐方向导航、Tab环和提示；编译0错误0警告，原生保存重载后45控件与父类再次读回。可访问性零错误、新控件零警告，九项原有小字号警告保留记录；工具不支持父类默认焦点属性，因此未把静态焦点审计冒充完整可达性证明。资产证据在 `Saved/Validation/WorldEntryFix20261015/LoginTestAccountsMonolith.json` 与 `LoginTestAccountsNativeReload.json`。

最终完整Editor构建 `9cba8b78-de12-4e2a-8a67-e5df71b00948` Passed/Eligible，Client/Server `65fe0732-83c8-4415-aae4-8fe5c1993da7`两个目标退出0。新增四项原生开发入口/账号回归，以及既有登录命名控件/密码显隐两项，均在最新39项聚合中真实Success。新Client Cook/Stage `ea610e38-13b5-451b-af74-92cbe65c500f`通过，两个可见客户端已显式启用开发工具与十账号入口；人工点击、真实认证及门户交互尚待体验，不由进程启动或控件测试代替。汇总见 `Docs/Implementation/WorldAndDevelopmentFollowup_20261015.md`。
