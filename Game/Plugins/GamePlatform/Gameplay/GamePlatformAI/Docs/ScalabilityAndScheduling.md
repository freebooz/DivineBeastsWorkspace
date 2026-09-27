# ScalabilityAndScheduling（伸缩与调度）

共享模型已定义 Critical、Active、Background、Dormant 四档；AIDefinition UpdateProfile提供 DecisionInterval、MoveRefreshInterval、MoveRefreshDistance、AttackRetryBackoff 和 DefaultTier。

当前服务器使用有界 Decision Timer 和事件驱动 Perception；不会每帧扫描全部玩家、每帧全量目标排序、每帧提交Move或每帧调用后端。候选集合由 MaxCandidates + MemorySeconds约束。

四档动态频率调度尚未做性能数据驱动实现；避免使用未经测量的“最佳固定倍数”。后续压力测试后才根据 Server CPU/GameThread/Perception/Navigation数据决定各档频率。

1/10/20/50 AI压力、服务器CPU和帧时间当前均未执行。
