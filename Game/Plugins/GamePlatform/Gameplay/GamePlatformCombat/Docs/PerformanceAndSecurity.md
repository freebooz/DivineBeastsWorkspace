# PerformanceAndSecurity（性能与安全）

CombatComponent 不启用 Tick；结算通过 GameplayEffect、Attribute callback、Tag event 和服务器命中时事件驱动。没有每帧扫描战斗参与者或每命中调用后端。

EventId 本地幂等缓存上限 256，避免无限增长。Hit Validation 限制距离、起点偏移和 Sweep 半径。Damage/Healing/Control 时长都验证有限数值和上限。

客户端没有任意 Damage、EffectClass、Kill、SetHealth Server RPC；Spec 的 EffectClass 由 Combat 内部固定为受信任 C++ GameplayEffect。

真实性能指标尚未测量：活跃 GE、Combat 事件率、属性复制、GameplayCue、CPU、内存和带宽均需 UE Insights/网络测试后再给结论。
