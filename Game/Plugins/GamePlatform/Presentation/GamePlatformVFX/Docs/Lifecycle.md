# Lifecycle（生命周期）

标准请求生命周期：

```text
Presentation已解析DefinitionId
→ Reserve Handle
→ GamePlatformData AcquireDefinition(World Lease)
→ Definition/VFXRuntime ready
→ Create NiagaraComponent（AutoActivate=false）
→ Register Instance
→ Bind OnSystemFinished
→ Schedule MaxLifetime Timer（如配置）
→ Activate
→ Finish / Stop / Corrected / Cancelled / Timeout
→ Remove Dedupe + Clear Timer + ReleaseDefinition + Remove Instance
```

规则：

- `Queued` 立即返回 Handle，允许加载完成前 Stop；
- `OnSystemFinished` 是正常自然结束的正式回收事件，Prune 不再承担主要生命周期；
- `MaxLifetimeSeconds` 由 Timer 实际执行，不等待下一次 Play；
- Stop 对 Pending、Active 和 Composite 子实例递归安全处理；
- WorldSubsystem Deinitialize 释放全部 Pending/Active/ExplicitPreload Definition Lease；
- Composite 延迟步骤执行前检查父 Handle，且每个 Child 再次通过统一 Budget Gate；
- Niagara Spawn 不自动激活，先绑定生命周期再运行，避免极短特效错过完成事件。
