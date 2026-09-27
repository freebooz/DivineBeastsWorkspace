# ServerViewAndAuthority（服务器视角与权威）

Client View（客户端视角）和 Server View（服务器权威视角）使用 SourceView（来源视角）显式区分。

当面板运行在 Authority World（权威世界，例如 Listen Server）时，可用同一只读 Provider（状态提供者）按 Server SourceView（服务器来源视角）采集。

当面板运行在纯 Client（客户端）时，V1 不发送自定义 Remote Debug RPC（远程调试RPC）。服务器权威视角使用 UE Gameplay Debugger（玩法调试器）的已有服务器采集与分类复制能力。这样避免：
- 把本地 Transform（变换）伪装为服务器状态。
- 新增任意远程查询面。
- 新建 Debug Actor（调试Actor）复制系统。

因此 Development-only Server Snapshot RPC（仅开发服务器快照RPC）的 Auth/RateLimit/Non-owner（认证/限流/非所有者限制）测试在 V1 为“不适用”，不是“通过”。