# 《神兽联盟》双模式插件开发入口 P0—P5 分步执行与验收（2026-10-12）

> **工作区**：本轮严格使用用户指定 **Cwebcodex** 所连接的 `E:\poject\feebooz\DivineBeastsWorkspace`，不是此前 HWebCodex 指向的 `E:\work\2026\DivineBeastsWorkspace`。本轮起始主分支HEAD为 `8d2bfe2738b25c72640475840348a885e0628e05`，开始时有9项其他任务未提交修改；不得覆盖、重置或声称前一工作区代码已经同步。
>
> **计划**：Debug（完整流程调试）默认使用正式登录入口；PluginDev（插件开发）仅开发构建、交互本地客户端显式启用，免登录进入插件开发门户。一个GameInstance、一份目录、不修改Shipping或Dedicated Server启动。

## P0—P1：启动模式与登录隔离

既有源码（本轮核查并保留，不复制）：
- `DivineBeastsApplicationFlowClient/Development/DivineBeastsPluginDemoMode.cpp`：由 `-DBAPluginDemo`、`-DBAPluginDemoMap` 精确地图值、非Shipping/Test、非Commandlet/Server及Foundation冲突条件共同控制旁路。
- `DivineBeastsApplicationUIAdapter::Initialize`：演示模式不调用 `StartFlow(false)`，不订阅正式认证流程。
- `DivineBeastsUIClientSubsystem::Initialize`：演示模式跳过正式登录RootLayout与业务页面。
- `DivineBeastsApplicationFlowSubsystem::StartFlow`：已存在开发分支防御式校验，需真实自动化复核。
- `DivineBeastsPluginDemoLaunchPolicyTests.cpp`：源码含原生启动策略自动化用例；是否在本轮Editor执行成功，须读取官方报告。
- `DefaultEngine.ini` 保持 `GameDefaultMap=/DBAFrontEndPack/Maps/L_DBA_FrontEnd`，不可更改为开发入口。

## P2—P3：门户资源、插件目录与演示进退

- **已有实现**：`DivineBeastsDemoClient（开发演示客户端模块）` 的 `UDivineBeastsDemoCatalogSubsystem` 使用 `IPluginManager（插件描述）` 和 `AssetRegistry（真实地图注册表）`，提供插件列表、功能地图、`OpenScenario` 和 `ReturnToHub`；`UDivineBeastsDemoWorldSubsystem` 在世界生命周期申请UI资产租约、卸载时清理请求和页面；`UDivineBeastsDemoHubWidget` 为真实门户Widget父类。
- **新增作者脚本**：`Tools/Unreal/PluginDemos/AuthorDBAPluginDemoHubMap.py`，通过UE5.8真实编辑器创建开发总览 `/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub`；绝不删除/覆盖已有地图，创建前校验默认Cook排除及项目身份。
- **新增资产复核脚本**：`Tools/Unreal/PluginDemos/ValidateDBAPluginDemoHubMap.py`，独立Editor加载真实UWorld并校验GameMode、PlayerStart与门户环境。
- **UI强制规则**：所有 `WBP_DBA_PluginDemoHub`、`WBP_DBA_PluginDemoOverlay`、`WBP_DBA_PluginDemoBirdFlock` 必须由Monolith MCP（用户界面编辑器服务）创建、编译、保存并重新打开。普通UE Python仅允许创建非UMG的关卡，不允许借助Python伪造Widget蓝图。
- **当前障碍**：Cwebcodex的Monolith MCP代理已经登记，但实际 `monolith_status` 拒绝操作并提示 Unreal Editor 未运行/未连接。曾启动本会话专属编辑器进程，但未得到可供Monolith操作的有效交互会话，已请求停止该无效进程，未触碰其他进程。
- 开发演示关卡的 `UI Widget` 未验证之前，只能说目录与导航实现具备，不得声称实际交互可用。

## P4：两模式统一启动器

本轮新建 `Build/Game/LaunchDBADevelopment.ps1`：`-Mode Debug`（默认）从真实 `L_DBA_FrontEnd` 启动并保留人工认证，`-Mode PluginDev` 复用既有 `RunPluginDemo.ps1` 的双重显式参数与地图完整性校验。必须传 `-Start`，仅接受 UE5.8 的明确EngineRoot/UE_ROOT，记录独立RunId而不覆盖历史证据；不传测试密码、会话票据或开发免登录标志给Debug。
- 编写脚本及消除对他人 `$LASTEXITCODE` 残留的依赖已经落盘。
- 由Runner旧版Windows PowerShell 5.1 解析无BOM中文 UTF8 会出现伪语法错误，应该以 `pwsh.exe` PowerShell 7（工程要求）独立验证，不得根据PS5乱码判定脚本不可用。

## P5：各类验收与不能代签事项

仍需依次完成：
1. Cwebcodex同一工程的三个目标适用编译、模块装配边界和原生模式策略用例；
2. UE真正地图创建、独立编辑器重载及开发/Shipping Cook隔离；
3. Monolith真实Widget蓝图、中文插件卡片、按插件列出地图/自动化可用性、点击OpenScenario/ReturnToHub、切换10次世界生命周期检查；
4. Debug真实从Login开始不自动认证；PluginDev真实从Portal开始且不发HTTP、不进入正式Session；
5. 人工视觉/键鼠/焦点及音效签审，自动化不能替代。

**本记录中的开发资源阶段未完成项必须据真实Job与Monolith连接继续更新，不能仅凭作者脚本存在宣布完成。**

## 2026-10-12 续做记录：门户蓝图接线能力与引擎资产制作

- 复用已有 `DivineBeastsDemoClient`，仅在其 `UDivineBeastsDemoHubWidget` 增加按插件名称、稳定ID、说明、三层分类及已发现地图的只读筛选API；不在输入变化时扫描资产、访问后端或创建重复目录。
- `UDivineBeastsDemoWorldSubsystem` 在真实Widget成功加载后设置 GameAndUI（游戏与界面共享输入）、焦点与鼠标可见；离开本世界撤销自身输入设置并恢复鼠标原状态。代码已落盘，编译及实测待验证。
- 新增 `TestDBAPluginDevDualMode.py` 的门户筛选和输入生命周期静态回归检查；此检查不证明Widget蓝图存在或UE运行成功。
- 本次开始核对时，正式地图 `L_DBA_PluginDemoHub.umap` 及 `WBP_DBA_PluginDemoHub.uasset` 均不存在；Monolith服务路由可解析，但无正在运行的UE编辑器实例，因此没有声称已创建蓝图。
- 首次尝试通过锁定 UE5.8 的 `UnrealEditor-Cmd.exe` 执行真实地图作者脚本，子进程退出码1，地图**未获生成确认**；需要基于项目编辑器日志排查后重试。不能以启动进程或作者脚本存在代替真实资源证据。
- 当前Cwebcodex Runner拥有其他会话持续执行的作业，禁止清理、停止或覆盖他人的资源与构建工件。本次新增代码需等待无冲突构建窗口统一验收。
- 已实际执行 `python Tests/Architecture/TestDBAPluginDevDualMode.py`：`Ran 10 tests in 0.003s / OK`，属于源码静态回归，不代表运行构建或视觉验收。
- 真实UE5.8日志 `Game/Saved/Logs/DivineBeastsArena.log` 在本次启动的第1726行明确记录：`无法找到模块“DivineBeastsDemoClient”，因此插件“DBAClient”加载失败`。编辑器在执行地图作者脚本前退出；直接障碍为新增客户端模块缺少当前Editor Target可加载的动态库，**不是**Monolith缺少UI动作。必须完成模块或编辑器目标构建并重启，再执行真实地图制作、Monolith Widget和回读。
- 本Runner未发现 `pwsh.exe`（PowerShell 7），统一启动脚本保留原来的PS7版本声明和拒绝静默降级原则；实际运行需补齐正式脚本环境后再测。

## 二进制阻塞排查与定向编译（同日后续）

- 初次定向构建 `DivineBeastsDemoClient` 报LNK2019（`FGamePlatformResult`），确认定义在 `GamePlatformCore/Private/Types/GamePlatformResult.cpp`，在模块 `PrivateDependencyModuleNames` 增加 `GamePlatformCore` 后重新构建，UBT Win64 Editor Development **Succeeded**。
- `-Module=DivineBeastsDemoClient` 仅输出DLL但不自动登记旧 `UnrealEditor.modules`，通过UE5.8原生 `UnrealBuildTool -Mode=WriteMetadata` 与已有 `TargetMetadata.json` 更新官方清单；核查 `DivineBeastsDemoClient` 已在清单内且BuildId为 `cf41249f-44dc-4956-bd09-4a42f079e86b`，不是人工伪造模块文件。
- 真实Editor装载随后报127（缺少导出函数）。使用VS `dumpbin` 将DemoClient导入符号与现有依赖DLL导出逐项对照，定位平台VFX新 `ValidateFlockingWeights` 和项目表现层6项鸟群接口；`GamePlatformVFXClient` 定向构建完成27动作 **Succeeded**，`DivineBeastsPresentationClient` 定向构建完成26动作 **Succeeded**，均没有为绕过错误改动业务算法。
- 地图作者命令仅在本进程尝试 `-DisablePlugins=DBAArena`（隔离不相关竞技插件，不变更正式Target或默认配置）；UE仍有未就绪编辑器模块，最近一次失败日志指出 `GamePlatformDeveloperTools` 找不到模块。已发起其Editor模块定向构建，最终状态须通过实际作业终态核定。
- **验收状态**：门户地图 `.umap` 与Widget `.uasset` 暂无生成证据、Monolith没有存活编辑器连接、统一编辑器全目标构建与Debug/PluginDev两模式人工运行未通过。保留已成功的分模块构建结论，绝不据此宣称端到端完成。



