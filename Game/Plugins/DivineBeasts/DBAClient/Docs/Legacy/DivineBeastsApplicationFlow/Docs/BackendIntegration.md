# BackendIntegration（后端集成）

正式Go服务入口仍为5个：gatewayservice、identityservice、playerdataservice、matchservice、gameservercontrolservice。

PlayerDataService 通过 applicationflow extension 增量提供 Profile、Roster、Create、Select；Gateway 根据认证身份转发，不接受客户端自报PlayerId。GameServerControlService通过 worldcontrol extension 提供世界Assignment和Ticket校验消费。

PostgreSQL保存profile、持久角色、Create幂等操作、Selection幂等操作以及world assignment/ticket消费。开发环境支持静态Allocator；Agones模式已实现基于 `allocation.agones.dev/v1 GameServerAllocation（Agones游戏服务器分配）` 的 Kubernetes API Adapter（适配器），按 Experience/Region 标签选择服务器并读取Map/World/Region元数据。当前Runner没有Go/Kubernetes/Agones运行环境，因此真实编译和集群联调仍为“未执行”。
