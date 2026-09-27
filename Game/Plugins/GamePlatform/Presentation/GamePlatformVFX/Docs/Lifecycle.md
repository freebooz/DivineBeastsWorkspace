# Lifecycle（生命周期）

请求生命周期：Semantic Request（语义请求）→ Resolve（解析）→ Reserve Handle（预留句柄）→ Definition 已加载则立即执行，否则异步预加载 → Niagara Spawn（生成）→ InstanceRegistry 登记 → Stop 或组件自动结束。

规则：
- Queued（已排队）结果立即返回 Handle，允许调用方在加载完成前 Stop。
- Stop 对 Pending（等待中）、Active（活动中）和 Composite 子实例均安全处理。
- WorldSubsystem Deinitialize（世界子系统反初始化）时取消所有预加载、停止实例、清空 Catalog。
- Composite 延迟回调执行前重新检查父 Handle 是否仍活动，避免切图或主动停止后重新生成子效果。
