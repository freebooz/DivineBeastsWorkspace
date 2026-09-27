# PerformanceAndScalability（性能与可扩展性）

资产扫描优先 FAssetData 和 Asset Registry 元数据，避免为了审计无差别 Load 全部 UObject。

10k Asset Benchmark（万资产基准）应记录资产数量、扫描耗时、峰值内存、报告写入耗时与机器配置。当前源码提供批量元数据扫描路径，但未在真实 UE5.8 Editor 环境执行万资产基准，因此验收状态必须是 未执行。

增量 PR 验证优先 Changed Files/受影响依赖；无法可靠解析变更时回退 Full Validation，不为了速度跳过阻断规则。