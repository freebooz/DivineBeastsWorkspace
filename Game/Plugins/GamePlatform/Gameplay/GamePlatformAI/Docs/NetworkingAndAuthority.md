# NetworkingAndAuthority（网络与权威）

`GamePlatformAIServer`模块类型为 ServerOnly。权威 AIController、AIPerception、BehaviorTree、目标选择、Navigation和Ability决策只在服务器世界运行；Client Target没有直接引用 AIServer 模块。UE5.8 AAIController在网络游戏中也是服务器侧控制器。

客户端接收 AI Pawn Replicated Movement（复制移动）、Combat属性/死亡和 `FGamePlatformAIStateSnapshot`必要公开状态；不运行 Brain/Perception，不修改 Target/Blackboard。

AIState Snapshot、AI Target Identity/Generation使用属性复制，为 Late Join 提供当前状态而不是重放过去感知事件。完整候选表、Blackboard、路径和调试数据不复制。

真实 Dedicated Server + 两客户端、Late Join、多PIE和 Client不创建AIController 的运行证据仍未执行。
