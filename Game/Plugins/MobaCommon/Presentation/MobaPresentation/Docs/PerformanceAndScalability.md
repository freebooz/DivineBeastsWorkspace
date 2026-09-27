# PerformanceAndScalability（性能与扩展）

5v5压力重点是Hit/Status事件率、Request分配、Context复制和Provider Dispatch（提供者分发）。

客户端适配不做每Tick状态扫描；Arena依赖RepNotify/Delegate，Combat依赖现有事件。去重缓存最多保留2048个FactId，避免无限增长。

避免UObject反射Map大拷贝、每请求生成不必要动态字符串、重复事实二次播放。

当前没有真实5v5压力数据，因此吞吐、CPU、内存和表现请求上限均为“未执行”，不得推断生产容量。
