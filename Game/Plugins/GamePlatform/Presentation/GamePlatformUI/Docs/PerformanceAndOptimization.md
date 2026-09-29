# PerformanceAndOptimization（性能与优化）

平台 UI 采用 Event-driven（事件驱动）更新，ViewModel 不依赖 Tick（逐帧更新）。正式页面应避免 Raw Attribute Binding（高频原始属性绑定）。

大列表使用 ListView Virtualization（列表虚拟化）；图片、WidgetClass 和辅助资产使用异步软引用加载。

高频 Feedback（反馈）使用 Service（服务）集中对象池：活动反馈上限48、池上限64、近期 OccurrenceId（发生标识）有界记忆256条；同一事实不会重复播放。WorldUI（世界UI）活动上限128、池上限128，仅有活动实例时启动30Hz集中投影Ticker，空闲时完全停止。

Invalidation（失效缓存）、Global Invalidation（全局失效）和 Retainer Panel（保留面板）只能基于 Slate Insights/Unreal Insights（界面/性能分析）数据启用，不能无测量堆叠。

当前 Runner 已确认 UE5.8 工具链位于 `D:\UnrealEngine-5.8.0-release`。本轮关键 UI 服务和项目 UI 源码已完成 `DivineBeastsArenaClient Win64 Development` 定向 UHT/C++ 编译；真实 UMG/Slate GPU/CPU 性能基线仍需在创建 Widget Blueprint 和实际场景后通过 Unreal Insights / Slate Insights 执行。
