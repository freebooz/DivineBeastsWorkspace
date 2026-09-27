# PerformanceAndOverhead（性能与开销）

RecordEvent fast path（事件记录快速路径）只做结构校验、采样、令牌桶限流、轻量尺寸估算和短锁有界入队；不做网络、资产加载或大 JSON 序列化。

序列化在 Batch→HTTP Transport（批次→HTTP传输）阶段发生。Buffer、Batch、Pending Network Batch、NATS reconnect buffer 都有显式上限。

已创建 Development-only 1000 Records（开发1000条记录）UE 测试框架，但当前没有实际 UE Runner，所以 1/10/100/1000 events/sec（每秒事件）CPU、allocation、bytes 和 overflow 性能数据均未执行，不能承诺指标。