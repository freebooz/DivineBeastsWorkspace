# PerformanceProfiling（性能分析）

最低性能场景为1v1、5v5、OpenWorld、Village和Android目标设备。必须分别记录 game thread、Niagara System实例、粒子/内存、EffectType裁剪、Pool重激活、共享Definition缓存和加载峰值。

当前代码已提供：

- `TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_Play)`；
- `TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_ExecuteDefinition)`；
- `TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_DefinitionLoaded)`；
- `stat GamePlatformVFX`：Play Requests、Rejected Requests、Dedupe Hits、Definition Cache Hits/Misses、Composite Children、Tracked Instances、Pending Instances、Peak Tracked Instances。

性能审核时至少记录：

```text
GT / RT / GPU frame time
Niagara System / Emitter / Particle count
Pool hit / reactivation cost
Definition Cache hit/miss
Pending Definition instances
Dedupe hit
Rejected by scalability
Composite child count
Peak tracked instances
EffectType cull / significance / budget scaling
Transparent overdraw / Ribbon / Light / Decal cost
```

更多 System 实例会增加 game thread 成本；Pooling 能减少创建成本但仍有 reactivation 代价，共享 Definition Cache 能减少 Data Lease/异步调度成本但会增加受控的 World 热缓存驻留，因此不能凭单一指标宣布性能达标。

当前 Runner 已具备 UE5.8 Win64 构建工具链，性能整改后的 GamePlatformVFXClient + GamePlatformVFXEditor 已定向编译通过；但仓库仍没有真实 Niagara System/EffectType/Definition 二进制资产，且未取得 1v1、5v5、OpenWorld、Village、Android 目标设备的 Insights 工件，所以真实性能基线仍为未执行。