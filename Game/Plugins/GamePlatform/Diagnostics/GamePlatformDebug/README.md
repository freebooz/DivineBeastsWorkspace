# GamePlatformDebug（游戏平台调试插件）

版本：1.0.0  
定位：Development/Test（开发/测试）专用的 UE5.8（虚幻引擎5.8）双端观察与诊断能力。Shipping（正式发布）明确裁剪。

## 已实现范围

- GamePlatformDebug（双端调试运行模块）。
- GamePlatformDebugClient（客户端调试模块）。
- DebugSnapshot / DebugTarget（调试快照/调试目标）。
- Command Registry / State Provider Registry（命令注册表/状态提供者注册表）。
- IConsoleManager（控制台管理器）统一 `gp.Debug.*` 命令。
- Gameplay Debugger（玩法调试器）自定义 `GP.*` 分类。
- World / Character / GAS / Combat / AI / Navigation / Network / Online / Session / Loading / Telemetry（世界/角色/技能系统/战斗/人工智能/导航/网络/在线/会话/加载/遥测）诊断。
- Client View / Server View（客户端视角/服务器权威视角）边界。
- Slate Client Panel（Slate客户端调试面板），Manual/1Hz/5Hz/10Hz（手动/刷新频率）。
- Sensitive Filter（敏感字段过滤）、Snapshot Bounds（快照上限）、Target Lifecycle（目标生命周期）。
- Shipping Gate（正式发布门禁）：Module Allow-list（模块允许列表）+ Target Disable（目标禁用）+ `UE_BUILD_SHIPPING` 条件编译。
- Core Automation Tests（核心自动化测试）与 PowerShell Validation（PowerShell验证）入口。

## 安全边界

本插件不是 Production Admin（生产管理后台）、GM（游戏管理员）系统、业务 API（业务接口）或 Cheat（作弊）系统。

不存在：GrantItem（发放物品）、GrantXP（发放经验）、SetHealth/Revive（设置生命/复活）、Payment 修改（支付修改）、任意 SQL/Shell/File/URL（数据库语句/系统命令/文件/网址）执行。

V1（第一版）不新增自定义 Remote Debug RPC（远程调试RPC）。服务器权威状态优先使用 UE Gameplay Debugger（虚幻玩法调试器）原生复制链路，避免第二套 Debug Actor（调试Actor）复制系统。

新增 Go（Go语言）业务后端接口：无。

## 当前验证状态

当前正式结构入口为 `Tests/Architecture/ValidateDesignBaseline.ps1` 和 `Tests/Architecture/PluginCompositionAudit.psm1`。旧说明中的Test-PluginLayers脚本在本分支不存在，旧859项计数不能作为当前验收证据。

2026-09-30整改补齐真实插件依赖及NoPCH所需WeakObjectPtr/Pawn头和UE5.8指针转换；实际结果统一见 `Docs/Implementation/GamePlatformDesignRemediation/ExecutionProgress.md`（工作空间根目录）。结构通过不替代下面的运行验收。

未执行：UE5.8 Development/Test/Shipping Build（开发/测试/正式构建）、Cook（烘焙）、PIE（编辑器运行）、Dedicated Server + 2 Clients（专用服务器+双客户端）、Unreal Insights / Networking Insights（虚幻分析器/网络分析器）真实运行验证。

人工审查：未执行，AI 不代签。

## 文档

`Docs/` 下包含 23 份专题正文，加本 README（说明文档），共 24 类文档。测试证据见 `Docs/TestingAndEvidence.md`（测试与证据），人工检查清单见 `Docs/ManualReview.md`（人工审查）。
