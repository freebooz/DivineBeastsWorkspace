# Architecture（架构）

GamePlatformDebug（游戏平台调试插件）位于 GameFoundation/Diagnostics（跨游戏基础层/诊断分类），不反向依赖 MobaCommon（MOBA通用层）或 DivineBeasts（神兽联盟项目层）。

模块：
- GamePlatformDebug（双端调试运行模块）：中立 DebugSnapshot（调试快照）、DebugTarget（调试目标）、Command Registry（命令注册表）、State Provider Registry（状态提供者注册表）、Gameplay Debugger（玩法调试器）分类。
- GamePlatformDebugClient（客户端调试模块）：Development/Test（开发/测试）客户端 Overlay Panel（叠加调试面板）。

数据流：
Target（弱目标引用） → State Provider（只读采集） → DebugSnapshot（有界中立快照） → Sensitive Filter（敏感过滤） → Console / Gameplay Debugger / Client Panel（控制台/玩法调试器/客户端面板）。

权威边界：
- Gameplay Authority（玩法权威）：UE Dedicated Server（虚幻专用服务器）。
- Business Authority（业务权威）：Go/PostgreSQL（Go语言/PostgreSQL数据库）。
- Debug（调试）仅观察与诊断，不拥有 Gameplay 或业务状态。

V1（第一版）不实现自定义 Remote Debug RPC（远程调试RPC）。服务器权威数据优先通过 UE5.8 Gameplay Debugger（玩法调试器）已有复制链路提供，避免自建第二套复制系统。