# Security（安全）

Telemetry Payload 一律视为不可信输入。Gateway/Server Handler 校验认证、Content-Type、Content-Encoding、Body Size、Rate Limit，并使用 json.Decoder DisallowUnknownFields（禁止未知字段）。

客户端身份由 Gateway 覆盖为后端 HMAC 伪匿名标识；客户端不能伪造 ServerInstanceId/ServerRole。服务器身份字段由受保护请求头覆盖上传 Context。

Forbidden Attribute、Metric 高基数标签、Timestamp Skew、Event/Batch Size 在后端再次校验。

第一版明确拒绝压缩请求，因此没有 gzip 解压路径。NATS subject 固定，不允许 Payload 控制任意转发主题。

当前 Server Ingest 使用共享内部 Token，尚未接正式 Server Registry/每实例凭据验证；该项必须在生产验收前补齐。