# PerformanceAndScalability（性能与扩展）

最终性能基线至少需要：

1v1：2 Clients。
2v2：4 Clients。
3v3：6 Clients。
4v4：8 Clients。
5v5：10 Clients。

记录：
- Server frame time。
- CPU / memory。
- Network / replication bytes。
- Arena project policy validation cost。
- Hero selection/ready/countdown状态复制。
- MatchResult payload大小。
- PostMatch transfer耗时。

项目Runtime不Tick扫描；Project Catalog是静态小数组；Assignment/Revision检查只在比赛装载/准入路径执行。

当前没有UE Dedicated Server环境和10 Client压力证据，因此性能基线为未执行，不承诺未经实测容量。
