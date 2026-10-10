# EconomyBoundary（经济系统边界）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

`Backend/gameplatform/economy/port.go`只定义最小 Debit/Credit、WalletId、CurrencyId、LedgerEntry、EconomyOperationId（扣款/入账/钱包/币种/账本/操作编号）端口。

当前没有业务明确的 SoftCurrency（软货币）商城目录，也没有正式 Wallet/Ledger（钱包/账本）持久实现，因此 PlayerData 注入的 Economy Port 为 nil，SoftCurrency Order 会安全返回 SoftCurrencyUnavailable。

不把余额塞进 Inventory（背包），不使用 Commerce Order 表冒充钱包账本，不为架构完整强行创建虚拟币种。
