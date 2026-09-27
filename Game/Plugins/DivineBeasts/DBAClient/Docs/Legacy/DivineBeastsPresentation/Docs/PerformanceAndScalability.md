# PerformanceAndScalability（性能与伸缩）

平台Context/Catalog解析使用小规模LocalPlayer内存注册表；项目请求去重最多保留2048个RequestId，避免无界增长。

项目Client不Tick，不主动轮询Provider，不自行加载大型表现资源。Context只在流程/World/Character变化时更新；Interaction/Village反馈来自事件。

未来应实测：OpenWorld Interaction事件压力、Village反馈、Arena 5v5高频MOBA语义、Catalog条目规模、ContentPack反复激活/卸载、ProviderMissing和预加载延迟。

当前无UE运行环境和真实表现资源，因此性能基线为“未执行”。
