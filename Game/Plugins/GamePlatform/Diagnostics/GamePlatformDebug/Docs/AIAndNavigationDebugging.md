# AIAndNavigationDebugging（AI与导航调试）

AI Provider（人工智能状态提供者）使用共享 Runtime（运行时）模块公开的 FGamePlatformAIStateSnapshot（AI状态快照），显示 AIEntityId（AI实体编号）、AIDefinitionId（AI定义编号）、PublicState（公开状态）、Target（目标）、Movement/Combat Intent（移动/战斗意图）、StateRevision（状态修订）和 AIInstanceGeneration（实例代次）。

Blackboard（黑板）、Perception（感知）、BehaviorTree（行为树）、EQS（环境查询系统）与 NavMesh（导航网格）的深度数据继续使用 UE Gameplay Debugger 原生服务器能力；共享调试模块不依赖 GamePlatformAIServer（AI服务器模块）。

Navigation Provider（导航状态提供者）只检查 NavigationSystem（导航系统）可用性与选中目标位置。LastRequestId（最近请求编号）、PathStatus（路径状态）、Cost/Length（成本/长度）、Filter（过滤器）、Invoker（调用者）、SmartLink（智能链接）在没有安全共享接口时显示 N/A（不可用），不会为“调试”重新计算路径。