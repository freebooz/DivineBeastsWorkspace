# Security（安全）

Telemetry Payload 一律视为不可信输入。当前 UE 发送端已做 Schema/Privacy/Size 校验、Shipping HTTPS 门禁和动态凭据注入；未来 Gateway/Server Handler 仍必须再次校验认证、Content-Type、Content-Encoding、Body Size、Rate Limit，并使用 json.Decoder DisallowUnknownFields（禁止未知字段）。

目标后端必须把客户端身份覆盖为 HMAC 伪匿名标识；客户端 Bootstrap 当前不会把原始 AccountId 写入 Telemetry，只使用认证代次/随机 GUID 作为本地采样会话键。服务器身份字段未来必须由受保护请求头和 Server Registry 覆盖上传 Context。

Forbidden Attribute、Metric 高基数标签、Timestamp Skew、Event/Batch Size 必须在未来后端再次校验。当前真实后端只有 telemetry 领域占位，尚未具备这些校验。

未来后端第一版建议明确拒绝压缩请求，因此不引入 gzip 解压路径；规划中的 NATS subject 必须固定，不允许 Payload 控制任意转发主题。当前 NATS 未实现。

当前 UE Server Bootstrap 会动态读取共享内部 Token 构造目标请求，但 Server Ingest 本身尚未实现；正式生产前必须补齐 Server Registry/每实例凭据验证，不能只依赖共享 Token。