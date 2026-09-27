# API（接口）

公开 C++（C++语言）接口：
- Types/GamePlatformDebugTypes.h（调试类型）：FGamePlatformDebugSnapshot（调试快照）、FGamePlatformDebugField（调试字段）、FGamePlatformDebugTarget（调试目标）、FGamePlatformDebugCollectContext（采集上下文）、FGamePlatformDebugCommandDescriptor（命令描述）。
- State/GamePlatformDebugStateProvider.h（状态提供者接口）：IGamePlatformDebugStateProvider。
- Registry/GamePlatformDebugRegistry.h（调试注册表）：Provider（状态提供者）、命令元数据、目标身份、快照采集、敏感过滤。

IGamePlatformDebugStateProvider（平台调试状态提供接口）必须：
1. GetProviderId（获取提供者编号）。
2. CanCollect（判断是否可采集）。
3. CollectSnapshot（采集快照），不得产生 Gameplay 副作用。
4. GetEstimatedCost（获取预计成本），返回 Cheap/Moderate/Expensive（低/中/高成本）。

接口不提供 GrantItem（发放物品）、GrantXP（发放经验）、SetHealth（设置生命）、Teleport（传送）、SQL（数据库语句）或 Shell（系统命令）能力。