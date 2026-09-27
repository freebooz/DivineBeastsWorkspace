# Architecture（架构）

`GamePlatformAI（游戏平台人工智能插件）`包含两个正式模块：`GamePlatformAI（AI共享运行模块）`为 Runtime，承载 Definition（定义）、公开 Snapshot（快照）、Target Identity（目标身份）、Generation（代次）、配置和稳定排序；`GamePlatformAIServer（AI服务器执行模块）`为 ServerOnly，承载 AIController、Perception、BehaviorTree/Blackboard、Navigation 和战斗决策。

共享模块不依赖 AIModule、NavigationSystem、Combat、AbilitySystem、Interaction、UI/VFX/InputClient；服务器模块只添加实际使用的 AIModule、NavigationSystem、GamePlatformData、GamePlatformAbilitySystem、GamePlatformCombat。当前没有 AI Interaction 行为，因此没有强加 GamePlatformInteraction 依赖。

客户端只接收 Pawn、Movement 和 `FGamePlatformAIStateSnapshot（AI状态快照）`；权威 AIController/Perception/Brain 仅存在 ServerOnly 模块。UE5.8 官方也规定 AAIController 在网络游戏中只存在服务器。

当前 `GamePlatformWorld`/`GamePlatformCore`仍是骨架，因此 HomeRegion/World能力只保留中立 ID/位置边界，没有伪造已完成的 WorldContext API。
