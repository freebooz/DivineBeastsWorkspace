# BackendIngestion（后端接入）

Go Ingest Application（接入应用）执行 MaxRequestBytes、MaxBatchEvents/Metrics、MaxEventBytes、MaxAttributes、MaxStringBytes、MaxTimestampSkew 等限制。

安全/尺寸级错误整批拒绝；Event/Metric schema/privacy（事件/指标结构/隐私）错误按记录逐条拒绝，返回 accepted/rejected counts。

Gateway 接入要求正式 PlayerAuthenticator；Server 接入要求内部 Bearer Token 和 ServerInstanceId/ServerRole。两路都覆盖 UE 上传的不可信身份字段。

GatewayService（网关服务）和 GameServerControlService（游戏服务器控制服务）还使用 `RequestObserver（请求观测器）`记录 backend.request.duration_ms/backend.request.error_total：业务响应完成后只做非阻塞固定容量 channel（通道）入队，后台每秒/满128条时直接发布 NATS；队列满或发布失败只增加 Dropped（丢弃）计数，不改变业务响应。

第一版明确拒绝 compressed Content-Encoding（压缩编码），因此没有 gzip 解压路径，也不存在压缩炸弹解压逻辑。