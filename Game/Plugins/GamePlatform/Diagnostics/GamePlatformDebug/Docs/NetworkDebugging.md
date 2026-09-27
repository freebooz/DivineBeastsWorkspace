# NetworkDebugging（网络调试）

Network Provider（网络状态提供者）只输出可可靠读取的摘要：
NetMode（网络模式）、Connection（连接状态）、RTT（往返时延）。

PacketLoss（丢包）、Bytes（字节）、RPC Rate（RPC速率）、Correction（修正）和 Replication（复制）在当前共享公开接口无法可靠读取时明确显示 N/A（不可用），不制造模拟值。

深度网络分析复用 UE5.8：
- net.* Console Variables/Commands（网络控制台变量/命令）。
- Network Trace（网络追踪）。
- Networking Insights（网络分析器）。

推荐 Development（开发）启动参数：-trace=net -NetTrace=1。实际可用参数最终以锁定 UE5.8 源码和运行验证为准。插件不解析全部 .utrace（虚幻追踪文件）。