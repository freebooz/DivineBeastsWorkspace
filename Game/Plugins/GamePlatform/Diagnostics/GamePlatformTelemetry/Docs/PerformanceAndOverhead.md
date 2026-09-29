# PerformanceAndOverhead（性能与开销）

RecordEvent/RecordMetric fast path（事件/指标记录快速路径）只做 Schema 类型/隐私校验、采样、令牌桶限流、轻量尺寸估算和短锁有界入队；不做资产加载。Event 和 Metric 都有 Schema 级速率上限，`client.frame_ms/server.frame_ms` 默认最多稳定采样 10 条/秒、Burst 20，避免按帧率无限挤占 Buffer。

Flush（刷新）不再常驻每5秒唤醒：Buffer 首次有数据才安排一次性 Deadline，到达 Batch 条数/字节阈值会立即刷新；单次最多提交 `MaxFlushBatchesPerPass` 个 Batch，剩余数据再安排下一次 Deadline。Batch 以第一条记录 Context 为公共 SourceContext，Context 变化自然切批，HTTP JSON 不再为每条 Event/Metric 重复序列化完整 Context，并在最终 UTF-8 Payload 上再次执行硬字节上限。

BoundedBuffer 当前仍使用 TArray，有界容量和优先级 Drop 已生效，但高压情况下 `RemoveAt` 仍有线性搬移成本；后续性能阶段可替换为 Ring Buffer（环形缓冲），本轮未为了形式扩大重构。Development-only 1000 Records 测试仍只验证有界性，1/10/100/1000 events/sec 的 CPU、allocation、真实 HTTP 字节和 overflow 基准尚未形成运行数据，不能承诺具体性能指标。