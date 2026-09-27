# PerformanceAndOverhead（性能与开销）

性能原则：
- Client Panel（客户端面板）只提供 Manual/1/5/10Hz（手动/每秒1/5/10次），默认 1Hz。
- Panel 关闭时删除 FTSTicker（核心Ticker），停止面板周期采集。
- Gameplay Debugger（玩法调试器）只在用户启用分类时复制该分类数据。
- Provider（状态提供者）标记 Cheap/Moderate/Expensive（低/中/高成本）；高成本只允许手动/低频上下文。
- Snapshot（快照）有字段数、单字符串、Payload（负载）上限。
- 不每帧 GetAllActors（获取全部Actor）。
- 不每帧展开全部 GameplayEffect（玩法效果）。
- 不全量扫描 World Partition Cell（世界分区单元）。
- 不为诊断重新计算 Navigation Path（导航路径）。

真实性能基准尚未在 UE5.8 运行环境执行，因此不写具体 CPU/带宽“通过”结论。