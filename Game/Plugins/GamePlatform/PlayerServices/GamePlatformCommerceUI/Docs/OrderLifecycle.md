# OrderLifecycle（订单生命周期）

订单状态机集中由 Commerce Domain（商城领域）定义：
Created → AwaitingPayment → PaymentConfirmed → FulfillmentPending → Fulfilled。

失败/边界状态包括 Cancelled、Expired、PaymentFailed、FulfillmentFailed、RefundPending、Refunded。

非法跨状态转换由 `ValidateOrderTransition（订单状态转换校验）`拒绝。订单以 PurchaseIntentId 唯一创建；重复 BeginPurchase（开始购买）返回现有订单，不创建第二单。

客户端唯一“购买成功”条件是后端返回 OrderState=fulfilled、PaymentState=confirmed、FulfillmentState=fulfilled。仅 PaymentConfirmed 时 UI 必须显示“支付已确认，奖励处理中”等待状态。