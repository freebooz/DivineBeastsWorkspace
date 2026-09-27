# API（接口）

Gateway（网关）客户端接口包括：
- `GET /v1/commerce/catalog（获取商城目录）`
- `POST /v1/commerce/intents（创建购买意图）`
- `POST /v1/commerce/orders（创建/恢复订单）`
- `GET /v1/commerce/orders/{orderId}（查询订单）`
- `GET /v1/commerce/orders（最近订单）`
- `POST /v1/commerce/orders/{orderId}/receipt（提交支付凭据）`
- `POST /v1/commerce/orders/{orderId}/reconcile（订单对账）`

创建 Intent（意图）的外部输入严格只有 OfferId、Quantity、RequestId（报价编号、数量、请求编号）。PlayerId（玩家编号）来自认证上下文，可信 CharacterId（角色编号）只能来自可选认证扩展。客户端不能提交 Price、Currency、RewardSet、PaymentSuccess（价格、币种、奖励集、支付成功）。

Receipt（支付凭据）只是待验证数据，只有后端 Provider Verify（支付提供器验证）与订单金额/币种核对后才能确认 Payment。