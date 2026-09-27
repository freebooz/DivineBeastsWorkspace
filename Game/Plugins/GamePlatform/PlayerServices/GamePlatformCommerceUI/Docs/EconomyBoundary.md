# EconomyBoundary（经济系统边界）

`Backend/gameplatform/economy/port.go`只定义最小 Debit/Credit、WalletId、CurrencyId、LedgerEntry、EconomyOperationId（扣款/入账/钱包/币种/账本/操作编号）端口。

当前没有业务明确的 SoftCurrency（软货币）商城目录，也没有正式 Wallet/Ledger（钱包/账本）持久实现，因此 PlayerData 注入的 Economy Port 为 nil，SoftCurrency Order 会安全返回 SoftCurrencyUnavailable。

不把余额塞进 Inventory（背包），不使用 Commerce Order 表冒充钱包账本，不为架构完整强行创建虚拟币种。