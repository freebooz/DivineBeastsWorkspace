# OnlineSessionLoadingDebugging（在线/会话/加载调试）

Online（在线服务）、Session（会话）和 Loading（加载）当前相关平台插件仍未暴露满足本调试插件需要的稳定只读诊断适配器，因此 Provider（状态提供者）已建立结构与字段，但真实值统一显示 N/A（不可用）及原因。

Online 禁止输出 Token/Cookie/Authorization Header/Secret Query（令牌/Cookie/授权头/敏感查询）。

Session 预留安全字段：
SessionId摘要、MatchId（比赛编号）、Assignment（分配）、ServerRole（服务器角色）、Destination（目标）、TransferState（迁移状态）、TicketId摘要、ExpiresAt（过期时间）、Consumed（已消费）。
不输出 TransferTicket（迁移票据）的完整签名或 Nonce（随机数）。

Loading 预留 LoadingState（加载状态）、BlockingReason（阻塞原因）、SuppressToken数量、AssetLeases（资产租约）、PendingDefinitions（待加载定义）、ClientReady（客户端就绪）、ServerActive Gate（服务器激活门禁）和 TimeoutReason（超时原因）。