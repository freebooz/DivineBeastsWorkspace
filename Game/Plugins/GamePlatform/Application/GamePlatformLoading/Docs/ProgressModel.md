# 进度模型

实现位于`Private/Operations/LoadingPolicy.h`：

`OverallProgress01 = Σ(Task.Weight × Task.Progress) / Σ(Task.Weight)`。

启动前所有权重严格大于0且有限，总和也须有限；图最大256项，运行时不增删任务。单任务报告先拒绝NaN/无穷，再Clamp到0..1，再取历史最大值。回退保留同一逻辑任务显示进度上界，不能因为重新尝试倒退；但尝试代次及bIsFallback明确说明正在执行回退。

可选失败权重不移除、不重新归一化，不虚构成功进度。它失败前报告0则仍贡献0，因此Ready可能小于100%。相反，运行中任务报告1只改变显示，不改变状态，所以100%可能仍不Ready。

Data无法提供真实字节级进度时只有Pending=0、实际成功=1；世界条件也是离散0/1。没有每帧平滑、固定延时到100或后台猜测比例。UI将来可对显示独立平滑，但不得回写此快照或修改Ready判定。

运行时进度采样最多 20Hz，并且只有真实 Loading 操作存在时才注册主动 Ticker；空闲 GameInstance 为零轮询。Ready 后若资源仍由操作持有，只保留 2Hz 弱 Owner 回收监视，不再 20Hz Poll 已成功任务。状态订阅只在快照变脏时发布，不为 UI 动画制造每帧事件；UI 的视觉平滑必须在客户端表现层独立完成。

`GetLoadingDiagnostics()` 可读取 `TotalTickerExecutions`、`TotalTaskPolls`、`TotalSnapshotsPublished`、`TotalSubscriberCallbacks`、`LastTickMilliseconds` 和 `MaxTickMilliseconds`，用于后续性能回归；Loading 本身不依赖 Telemetry（遥测）插件。