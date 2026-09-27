# PaymentProviderBoundary（支付提供器边界）

`PaymentProvider（支付提供器端口）`定义 CreatePaymentSession、VerifyReceipt、VerifyCallback（创建支付会话、验证凭据、验证回调）。

当前没有已批准 Production Provider（生产支付提供器），因此 Production Payment=未执行。生产环境样例 `COMMERCE_PAYMENT_PROVIDER=none`。

`DevelopmentFakePaymentProvider（开发假支付提供器）`只支持 Development/Test，支持 success/failure/timeout（成功/失败/超时）测试；在 production/shipping 环境初始化直接失败，`IsProduction=false`，会话带 `NON-PRODUCTION PAYMENT PROVIDER`标识。

客户端 Provider SDK 返回值或 Fake Provider UI 结果都不是最终支付事实；必须由后端 Verify 后更新订单。