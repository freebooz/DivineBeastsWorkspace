# PerformanceAndOptimization（性能与优化）

平台 UI 采用 Event-driven（事件驱动）更新，ViewModel 不依赖 Tick（逐帧更新）。正式页面应避免 Raw Attribute Binding（高频原始属性绑定）。

大列表使用 ListView Virtualization（列表虚拟化）；图片、WidgetClass 和辅助资产使用异步软引用加载。

Invalidation（失效缓存）、Global Invalidation（全局失效）和 Retainer Panel（保留面板）只能基于 Slate Insights/Unreal Insights（界面/性能分析）数据启用，不能无测量堆叠。

当前无 UE 工具链，UMG/Slate 性能基线状态为“未执行”。
