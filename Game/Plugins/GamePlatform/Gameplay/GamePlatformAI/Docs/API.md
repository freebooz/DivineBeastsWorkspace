# API（公开接口）

共享 API：`UGamePlatformAIDefinition（AI定义资产）`、`UGamePlatformAIStateComponent（AI公开状态组件）`、`UGamePlatformAITargetComponent（AI目标组件）`、`IGamePlatformAITargetEligibilityProvider（目标资格接口）`、`FGamePlatformAITargetSelectionRules（稳定排序规则）`和中立状态/Profile结构。

服务器 API：`AGamePlatformAIController（平台AI控制器）`、`UGamePlatformAITargetRegistrySubsystem（AI目标注册世界子系统）`、`UBTTask_GamePlatformAITryAttack（行为树尝试攻击任务）`和 Blackboard Key 常量。

AIController公开的服务器动作只包括 `TryAttackCurrentTarget（尝试攻击当前目标）`与 `ForceDecisionUpdate（强制刷新决策）`；客户端没有设置 Target、Blackboard、Health 或 Brain 的权威接口。

AIDefinition 的 BehaviorTree/Blackboard/StateTree 为软资产路径，通过 GamePlatformData 的统一 Asset Loader 加载，不建立第二套 AssetManager。
