# BackendBoundary（后端边界）

本插件新增 Go（Go语言）业务后端接口：无。

可读范围仅限 UE（虚幻引擎）进程内已有安全公开状态，以及后续若存在的现有只读 Health/Diagnostics（健康/诊断）接口。

明确禁止：
DebugService（调试微服务）、SQL Endpoint（数据库语句接口）、Grant Item/XP/Entitlement（发物品/经验/权益）、修改 Order/Payment（订单/支付）、签发 Transfer Ticket（迁移票据）。

Debug（调试）不成为 Business Authority（业务权威）。Go/PostgreSQL（Go语言/PostgreSQL数据库）的业务状态只能由正式业务服务修改。