# BackendIngestion（后端接入）

本文件描述的是目标 Backend Ingest（后端接入）合同，不代表当前已经实现。当前仓库仅有 `Backend/internal/modules/telemetry/doc.go` 领域占位；尚未存在真实 Handler/Repository/NATS Adapter 或 Shared telemetry batch schema。

后续 Go Ingest 必须执行 MaxRequestBytes、MaxBatchEvents/Metrics、MaxEventBytes、MaxAttributes、MaxStringBytes、MaxTimestampSkew 等限制。安全/尺寸级错误应整批拒绝；Event/Metric schema/privacy（事件/指标结构/隐私）错误按记录逐条拒绝并返回 accepted/rejected counts。

目标 Gateway 接入要求正式 PlayerAuthenticator；Server 接入要求内部 Bearer Token 与 Server Registry 身份核验。两路都必须覆盖 UE 上传的不可信身份字段。

规划中的 GatewayService/GameServerControlService `RequestObserver（请求观测器）`、固定容量 channel（通道）和 NATS 发布当前源码尚不存在；实现时必须保持非阻塞、有界、失败仅计 Dropped，不得改变原业务响应。

未来后端第一版仍建议明确拒绝 compressed Content-Encoding（压缩编码），避免在尚未建立严格解压限额前引入 gzip 解压路径和压缩炸弹风险。