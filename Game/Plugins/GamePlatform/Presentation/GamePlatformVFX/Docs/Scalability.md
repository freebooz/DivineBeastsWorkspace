# Scalability（性能伸缩）

三层性能控制：

1. `MaxActiveInstances`：软预算，普通 Combat/Status/Ambient 达到后拒绝；Critical 可以突破；
2. `HardMaxTrackedInstances`：绝对安全上限，Pending/Active/Composite Child 均计入，Critical 也不能突破；
3. Niagara Effect Type：资产侧执行距离、并发、显著性、质量档、虚拟化/裁剪等引擎能力。

Pooling 优先使用 Niagara 原生池，不自建第二套 Component Pool。

当前默认值：

```text
MaxActiveInstances = 256
HardMaxTrackedInstances = 512
MaxPendingInstancePreloads = 64
```

这些值目前只是工程安全阈值，不是最终目标平台预算。正式数值必须通过 PC/Android、1v1/5v5/OpenWorld/Village 的 Niagara Debugger / Unreal Insights 数据批准。
