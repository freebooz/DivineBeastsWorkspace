# PaymentProviderBoundary（支付提供器边界）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

`PaymentProvider（支付提供器端口）`定义 CreatePaymentSession、VerifyReceipt、VerifyCallback（创建支付会话、验证凭据、验证回调）。

当前没有已批准 Production Provider（生产支付提供器），因此 Production Payment=未执行。生产环境样例 `COMMERCE_PAYMENT_PROVIDER=none`。

`DevelopmentFakePaymentProvider（开发假支付提供器）`只支持 Development/Test，支持 success/failure/timeout（成功/失败/超时）测试；在 production/shipping 环境初始化直接失败，`IsProduction=false`，会话带 `NON-PRODUCTION PAYMENT PROVIDER`标识。

客户端 Provider SDK 返回值或 Fake Provider UI 结果都不是最终支付事实；必须由后端 Verify 后更新订单。
