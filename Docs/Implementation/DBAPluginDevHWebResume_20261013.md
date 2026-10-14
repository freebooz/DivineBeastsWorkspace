# 《神兽联盟》双开发模式 P0—P5 HWebCodex 续执行记录（2026-10-13）

工作区：`E:\work\2026\DivineBeastsWorkspace`，锁定 UE5.8，HWebCodex 实际连接；本轮开始 Git HEAD `8c0b1a9b7884b579f79af245f493682f1ae2b13d`，开始时工作区 clean，之后有其他任务产生的独立未跟踪文件；本轮不修改、提交、推送、清理这些文件。

任务：Debug（完整流程调试）从现有前端登录；PluginDev（插件开发）仅显式授权开发客户端免登录进入插件开发门户，再按插件真实演示地图进入/返回。GamePlatform（平台层）→ MobaCommon（MOBA通用层）→ DivineBeasts（项目层），不建立第二套登录或演示框架。

## 一、已实施并获得实际证据

### P0/P1 唯一启动规则及登录隔离

- 已修改 `DivineBeastsUIClient（项目UI客户端）`：旧 `-DBALaunchMode=PluginDev` 参数**单独使用不能再绕过登录**；唯一授权入口是 `FDivineBeastsPluginDemoMode（项目插件演示模式策略）`，要求完整 `-DBAPluginDemo` 和准确 `-DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub`。
- `FDBADevelopmentLaunchMode（开发模式只读适配）` 统一向真正的ApplicationFlow演示策略查询；Test/Shipping、Dedicated Server、Commandlet、FoundationStandalone冲突或错误地图，全部保持Debug。应用UI适配器和项目UI子系统删除第二套重复条件，保留既有严格的`StartFlow(false)`阻断与UI登录旁路。
- 修改对应原生模式测试，以防单一旧参数绕过真实认证；旧参数独立使用返回Debug，正式双参数才返回PluginDev。**本轮Client Development（客户端开发构建）模块编译通过**：`Saved/Validation/DBAPluginDevDualMode/CanonicalPolicyClientBuild.log`。
- 本次双模式静态检查`Tests/Architecture/TestDBAPluginDevDualMode.py` **10/10成功**。静态检查不代替真正登录/免登录UI回归。

### P2/P3 演示门户地图与共享地图身份

- 已有UE5.8真实创建的 `/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub` 门户地图；前轮HWeb实际写盘21,961字节，独立Editor重新加载6个Actor成功，证据在`Saved/Validation/PluginDemos/Portal`。
- 复用项目现有`DivineBeastsDemoClient（项目演示客户端）`、`UDivineBeastsDemoCatalogSubsystem（插件演示目录）`、`UDivineBeastsDemoWorldSubsystem（世界演示导航）`与`UDivineBeastsDemoHubWidget（门户Widget父类）`；没有创建重复插件或重复GameInstance。
- 修正原目录把大部分`Foundation/PluginReviews`场景误标为DeveloperTools的问题：战斗、AI/交互/导航、MOBA竞技、SFX音效、十二生肖英雄共享场景，各自登记真实插件身份；一个.umap可被多个插件复用，但入口用`PluginId::真实地图长包路径`确保唯一，点击实际只按`MapAsset.GetLongPackageName（软资源实际长包名）`执行OpenLevel。未知插件/未启用/不存在的实际地图不提供可用进入状态；门户地图本身排除演示列表。
- 新增`Tests/Architecture/TestDBAPluginDemoMapRouting.py`，**5/5成功**；结合双模式10/10，当前静态及映射规则验证总计**15/15**。
- 实际`DivineBeastsDemoClient`客户端模块定向编译成功，见`Saved/Validation/DBAPluginDevDualMode/SharedReviewMapClientBuild.log`。

### P4 双模式启动器修复

- 原`Build/Game/LaunchDBADevelopment.ps1（双模式启动器）`向`RunPluginDemo.ps1（单一受控演示启动器）`使用字符串数组`@arguments`转发PowerShell参数，**真实启动时报Target参数校验错误**：把`-Start`解释为Target取值。
- 本轮采用PowerShell具名散列参数`@delegate`转发`Start/Target/WindowWidth/WindowHeight/RunId/EngineRoot/ClientExe`，不复制新的演示授权算法。
- 两个脚本的PowerShell 7语法解析均0错误；后续真实`-Mode PluginDev -Start -Target EditorGame`启动器返回0，创建独立RunId与Launch.json、启动本轮UE进程并传递正确地图及演示双参数，日志目录：`Saved/Validation/PluginDemos/Launch/468710c0-7ef8-4c17-8a15-13bdf6aff13d`。调用方按PID、进程启动时间及工程参数清理**仅自己的进程**。
- 实际图形编辑器启动在约84秒观察窗口内仍在引擎插件初始化，尚无演示目录日志；**不可判定已进入可交互门户**。

### P5 真实模块编译与启动尝试

- 真实完整客户端`DivineBeastsArenaClient Win64 Development` **920/920编译链接成功，正式UE5.8 Build.bat返回0**；日志`Saved/Validation/DBAPluginDevDualMode/CanonicalFullClientBuild.log`，耗时606.57秒，输出`Game/Binaries/Win64/DivineBeastsArenaClient.exe`。
- Editor目标`-Module=DivineBeastsUIClient -Module=DivineBeastsDemoClient -NoEngineChanges`本轮返回5（`FailedDueToEngineChange`），涉及共享UE Engine ImageCore/RHI等依赖产物，保留保护、不以覆盖共同引擎的方式强制编译；证据`Saved/Validation/DBAPluginDevDualMode/CanonicalModeEditorBuild.log`。
- 在`UnrealEditor.exe -game -nullrhi`的独立受控演示测试中，RunId `1681f2ba78584534a44f0c7bdbd832e7`，观察约210秒后日志仍在加载引擎模块，**没有发现插件目录初始化标志`神兽联盟演示目录发现插件`**；日志`Saved/Validation/DBAPluginDevDualMode/HeadlessPortal_1681f2ba78584534a44f0c7bdbd832e7/EditorGame.log`，启动子进程按身份安全清理。命令行内有地图路径不能冒充真实UWorld已加载。
- 实际`UnrealEditor-Cmd.exe`原生自动化用例报告`Saved/Validation/DBAPluginDevDualMode/NativePolicyTests/index.json`包含2项`DivineBeasts.PluginDev.LaunchMode.*`，2/2结果Success。但该Editor DLL时间是2026-10-12 22:58，旧于本轮2026-10-13 07:52修订源码，**故不作为本轮最新源码的原生运行验收**。第三项`DivineBeasts.PluginDemos.LaunchPolicy`未进入这一报告，不宣称3/3。
- Monolith MCP（项目UI视觉唯一合法创建工具）在本轮HWebCodex`mcp_tool list`两次均返回`servers=[]`；机器上虽然有`monolith_proxy.exe`进程，但不是可调用的正式连接。因此不存在真实`/Game/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub`、`WBP_DBA_PluginDemoOverlay`或`WBP_DBA_PluginDemoBirdFlock`蓝图。这部分严格按`AGENTS.md`不使用普通Unreal Python/文本/改后缀伪造。
- 资源范围：`DefaultGame.ini`（默认游戏配置）正式发布排除`/Game/Development/PluginDemos`；`Custom/PluginDemoClient/DefaultGame.ini`（演示客户端定制烘焙）才显式重新包含该目录。**本轮未重新执行正式客户端、演示客户端或服务器Cook，不能以配置存在替代包内结果**。

## 二、按阶段最终真实状态

| 阶段 | 真实状态 | 剩余缺口 |
|---|---|---|
| P0 唯一模式判定 | **代码和Client编译完成；静态校验通过** | 与本轮最新代码一致的Editor原生模式测试待Editor重新构建 |
| P1 登录流程隔离 | **规则已实现；Client编译通过** | Debug实界面完整启动与PluginDev后台无认证请求的端到端观察待验 |
| P2 开发门户地图 | **真实地图创建及独立重载完成** | Monolith创建/保存/重载正式门户UI仍未完成 |
| P3 插件目录映射 | **逻辑修改与Client编译完成；5项静态校验通过** | UE实际世界加载、UI点击和反复切换10次均待验 |
| P4 双模式启动器 | **真启动参数错误修复，具名转发已实测成功；PowerShell语法通过** | 等场景可运行后再审查完整流程/超时、Debug人工验证 |
| P5 目标验证 | **完整Client 920/920构建成功**；Editor/Monolith/人机验收未完成 | Editor构建阻断、真实门户Widget、客户端实际进退、开发Cook隔离、人工签审 |

## 三、Monolith恢复后的最小UI制作与验收

1. 先在连接的Monolith MCP中查看`monolith_status（编辑器和代理状态）`，确认真实 UE5.8 打开`Game/DivineBeastsArena.uproject`；若未注册服务先将Monolith暴露给HWebCode当前会话。不能仅依赖monolith_proxy进程。
2. 使用UI MCP创建并编译`WBP_DBA_PluginDemoHub`，父类`UDivineBeastsDemoHubWidget`，布局提供插件搜索/三层分类/启用状态/地图列表/进入场景/返回/诊断文字，C++现成接口为`GetDiscoveredPlugins`、`FilterDiscoveredPlugins`、`GetPluginScenarios`、`RequestOpenScenario`、`RequestReturnToHub`。
3. 使用同样合法工具创建`WBP_DBA_PluginDemoOverlay`，至少显式`ReturnToHub`按钮；鸟群专属`WBP_DBA_PluginDemoBirdFlock`的Species/Count/Flocking/Glide按钮和输入控件按`UDivineBeastsBirdFlockDemoWidget`真实BindWidget约束接线。
4. 每份Widget检查UClass父类、Widget树、焦点、输入模式、Monolith编译/保存及独立编辑器重新加载；完整更新对应Monolith资产制作记录。**若服务不可用则继续标记Blocked**。
5. 完成新Editor/Client验证后，实测至少VFX、SFX、PCG、Combat、Hero五类实例进出，十次进入/返回与音效/粒子和事件清理；Debug从真实登录开始、PluginDev无登录；执行发行/专服资源容器排除门禁。最后由用户或审核人完成设备及美术签收，自动化不得代签。

## 四、工具边界和安全

全程不修改Production GameDefaultMap、不创建第二套业务登录/网络系统、不写后端账号/令牌，保留GamePlatform→MobaCommon→DivineBeasts依赖方向。本轮变更留在项目Git工作区，未提交或推送。外部用户在其他项目/任务生成的文件未参与本次清理或重置。

**总结果：新增的真实客户端启动模式与目录修复编译通过，演示地图真实存在；可视门户、可点击导航与人工交付尚未完成。**
