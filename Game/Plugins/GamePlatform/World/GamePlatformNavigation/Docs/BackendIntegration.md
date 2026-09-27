# BackendIntegration（后端集成）

新增业务后端接口：无。新增 Go Navigation 微服务：无。数据库/Shared Contract 协议修改：无。

实时 NavMesh、点投影、路径查询、Invoker 和动态导航属于当前 UE World 的空间计算，由 UE Dedicated Server（专用服务器）执行。

Go 后端继续负责 World/Shard/Instance 分配和 Session 迁移；“从 OpenWorld 服务器 A 到 Village 服务器 B”属于跨服分配，不是 NavMesh 路径。

当前 Online/Session 真实认证链和专服导航运行均未执行，BackendIntegration 只记录边界，没有伪造后端联调成功。
