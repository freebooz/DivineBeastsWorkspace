# ConfigurationAndRun（配置与运行）

`DefaultGamePlatformAI.ini`包含安全上限：MaxSightRadius、MaxLoseSightRadius、MaxPerceptionAge、MinDecisionInterval、MaxTargetCandidates、MaxLeashRadius。实际 AI 行为参数来自 `UGamePlatformAIDefinition`。

第一版运行需要 Editor 创建 `DA_AI_FoundationMelee`、`BB_AI_Foundation`、`BT_AI_Foundation`及带 NavMeshBounds 的中立测试地图；AIDefinition 的 BehaviorTree/Blackboard使用软引用并通过 GamePlatformData加载。

Development C++ `AFoundationAITestCharacter`提供共享 Pawn、AIState、AITarget、ASC和Combat组件；ServerOnly WorldSubsystem在服务器为其动态创建 AIController，客户端无需链接 ServerOnly类。

当前 UE_ROOT不存在，因此无法实际启动 UnrealEditor/Server 完成资产制作和运行配置验证。
