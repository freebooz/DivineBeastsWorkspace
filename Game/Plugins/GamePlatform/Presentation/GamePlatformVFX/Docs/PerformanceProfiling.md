# PerformanceProfiling（性能分析）

最低性能场景为1v1、5v5、OpenWorld、Village和Android目标设备。必须分别记录game thread、Niagara system instances、粒子/内存、EffectType裁剪、Pool重激活和加载峰值。

更多System实例会增加game thread成本；Pooling能减少创建成本但仍有reactivation代价，因此不能只凭“启用池化”宣布性能达标。

当前Runner无UE5.8工具链和目标设备，所有真实性能基线均为未执行。静态Scalability门禁不等价于性能通过。