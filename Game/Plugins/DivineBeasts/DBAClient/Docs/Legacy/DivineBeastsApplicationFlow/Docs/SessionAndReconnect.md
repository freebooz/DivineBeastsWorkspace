# SessionAndReconnect（会话与重连）

GamePlatformSession 管理 Endpoint、ClientTravel、连接、Admission、Reconnect。项目 Flow 只观察 FGamePlatformSessionSnapshot（会话快照）。

失败时进入 Recovering，有界重试次数由 MaxRecoveryAttempts 控制。恢复不会复用旧 TransferTicket，而是重新 RequestWorldAssignment 获取新 Assignment/新 Ticket。

超过上限后返回安全 CharacterEntry，并报告 ReconnectExhausted。
