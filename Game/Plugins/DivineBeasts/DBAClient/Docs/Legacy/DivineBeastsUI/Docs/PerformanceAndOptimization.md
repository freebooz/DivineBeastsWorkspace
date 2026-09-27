# PerformanceAndOptimization（性能与优化）

项目UI采用事件驱动：

- ApplicationFlow View State事件
- Loading Snapshot事件
- Arena GameState/PlayerState事件
- Combat Event
- Interaction Focus事件

没有项目UI Tick扫描Gameplay Actor，也没有Raw Binding（原始绑定）热路径。

真实Widget出现后应遵循：

- ListView virtualization（列表虚拟化），用于角色列表/记分板。
- Async assets（异步资产）。
- Invalidation / Global Invalidation（失效缓存/全局失效缓存）按测量使用。
- Retainer Panel（保留面板）只在Slate Insights证明有收益时采用。
- 避免高频全树刷新。

当前无真实Widget/Slate运行，因此Slate Insights、PC/Android帧时间、内存、GC和页面切换性能基线均为“未执行”。
