# TransferAndTravel（迁移与切服）

TransferTicket（迁移票据）由后端签名并绑定 Player、Character、Session、Destination Server、Role、Experience、Assignment 和TTL。后端持久化Assignment并一次性消费Ticket，重复消费返回冲突。

项目 Flow 仅在 BeginLoadingForAssignment 中把不透明Ticket交给 GamePlatformSession；调用 BeginTransfer 后立即清除项目层 PendingTransferTicket 和 Endpoint。

ClientTravel 底层调用只允许存在于 GamePlatformSession，项目 Flow 不直接调用。
