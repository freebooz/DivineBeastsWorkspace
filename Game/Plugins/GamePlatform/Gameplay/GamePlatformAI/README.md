# GamePlatformAI（游戏平台人工智能插件）

跨游戏通用 NPC/怪物 AI 契约和服务器权威执行框架。正式包含 `GamePlatformAI（AI共享Runtime模块）`与 `GamePlatformAIServer（AI服务器ServerOnly模块）`。

已实现源码：AIDefinition、AIEntityId/Generation、复制公开状态、Target Eligibility/Generation、Sight/Hearing/Damage配置、服务器AIController、事件驱动Perception、服务器Sight Stimuli Source登记、稳定目标选择、BehaviorTree/Blackboard加载接口、Patrol/Investigate/Chase/Attack/ReturnHome状态、GamePlatformNavigation服务器服务接入＋BehaviorTree原生Move To、GAS标签攻击、Combat Death/Stun/Silence响应、生命周期清理，以及Development AI Pawn和非AI Target Pawn。AIDefinition加载前还会校验视野角、Hearing范围、更新频率、移动刷新、攻击退避和Attack/Preferred Range等安全边界。

第一版主Brain只采用 BehaviorTree + Blackboard；StateTree明确Unsupported。Threat/EQS/NavigationInvoker/Root控制/AI交互行为均未伪实现。

Runner当前没有可用且锁定的UE5.8工具链，且正式DA/BB/BT/NavMesh测试地图必须由Unreal Editor创建，因此UE编译、真实BehaviorTree运行、专服网络、压力测试与Cook仍待验证。

