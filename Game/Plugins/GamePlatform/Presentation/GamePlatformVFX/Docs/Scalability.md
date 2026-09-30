# Scalability（性能伸缩）

性能控制采用“平台分层软预算 + 绝对硬上限 + Niagara Effect Type”的组合：

1. `MaxAmbientInstances`：Ambient（环境）请求的累计软上限，默认96；优先为Status/Combat/Critical保留容量；
2. `MaxStatusInstances`：Status（状态）及Ambient请求的累计软上限，默认192；
3. `MaxActiveInstances`：Combat（战斗）及以下普通请求的总软预算，默认256；
4. `HardMaxTrackedInstances`：绝对安全上限，Pending/Active/Composite Child 均计入，Critical也不能突破，默认512；
5. Niagara Effect Type：资产侧执行距离、并发、显著性、质量档、虚拟化/裁剪等引擎能力。

预算是累计门槛而不是四个独立池。例如 Ambient 已占96后仍为 Combat 保留160个普通实例空间；Critical 可以突破256软预算，但不能突破512硬上限。

Pooling 优先使用 Niagara 原生池，不自建第二套 Component Pool。Importance（重要度）不决定池化资格；Critical 只影响预算/裁剪优先级，是否池化由全局开关和 Definition `bAllowPooling` 决定。

Definition 加载采用 World 共享缓存：

```text
MaxCachedDefinitions = 512
MaxPendingInstancePreloads = 64
```

同一个 Definition 的并发 Play / Preload 只持有一个 Data Lease。缓存达到上限后，只允许淘汰无Active实例、无Pending请求、无Preload Pin的最久未使用项；没有可淘汰项时拒绝新加载，避免无界资源常驻。

Effect Type 不是 Definition 字段“写了就生效”。Editor Validator 会加载 Default / Platform / Quality Niagara System，并要求其 `UNiagaraSystem::GetEffectType()` 与 Definition 声明的 EffectType 一致。

这些值目前仍是工程安全阈值，不是最终目标平台预算。正式数值必须通过 PC/Android、1v1/5v5/OpenWorld/Village 的 Niagara Debugger / Unreal Insights 数据批准。
