# PerformanceAndOverhead（性能与开销）

RecordEvent/RecordMetric fast path（事件/指标记录快速路径）只做 Schema 类型/隐私校验、采样、令牌桶限流、轻量尺寸估算和短锁有界入队；不做资产加载。Event 和 Metric 都有 Schema 级速率上限，`client.frame_ms/server.frame_ms` 默认最多稳定采样 10 条/秒、Burst 20，避免按帧率无限挤占 Buffer。

Flush（刷新）不再常驻每5秒唤醒：Buffer 首次有数据才安排一次性 Deadline，到达 Batch 条数/字节阈值会立即刷新；单次最多提交 `MaxFlushBatchesPerPass` 个 Batch，剩余数据再安排下一次 Deadline。Batch 以第一条记录 Context 为公共 SourceContext，Context 变化自然切批，HTTP JSON 不再为每条 Event/Metric 重复序列化完整 Context，并在最终 UTF-8 Payload 上再次执行硬字节上限。

BoundedBuffer 仍使用 TArray，但正常批次消费已改为 `HeadIndex（头索引）` 前移，不再每次 `RemoveAt(0,N)` 搬移全部剩余记录；被消费或从头部驱逐的槽位会立即清空 Event/Metric 内部字符串和数组资源，避免逻辑字节已下降但堆内存仍滞留。连续相同 Context 记录共享同一不可变 Context 快照，Event/Metric 自身入队后清空 Context 副本；Buffer 字节预算仍保守计入 Context 成本，避免共享实现掩盖真实内存上限。Counter/Gauge 在最近 64 条记录窗口内按同 Context/同 Labels 合并，Counter 累加、Gauge 保留最新值；Histogram/Duration 不合并，继续由 Schema 采样/限流，避免在正式桶协议缺失时伪造统计语义。仅在累计消费达到 256 条或已消费前缀超过数组一半时做一次摊销压缩。优先级驱逐仍保持稳定顺序，非头部候选在极端 Buffer Full 场景仍可能产生线性移动；若后续压力基准证明这部分成为瓶颈，再升级为 Ring Buffer（环形缓冲）/分优先级队列。Development-only 1000 Records 测试已经由 UE Automation 实际执行通过，但它只证明有界性；1/10/100/1000 events/sec 的 CPU、allocation、真实 HTTP 字节和 overflow 基准仍未形成完整数据，不能承诺具体性能指标。