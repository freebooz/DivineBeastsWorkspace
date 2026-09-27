# SecurityAndFraudResistance（安全与抗欺诈）

客户端不能提交 Price、Currency、RewardSet、PaymentSuccess、PlayerId（价格、币种、奖励集、支付成功、玩家编号）。PlayerId 来自正式认证上下文；可信 CharacterId（角色编号）仅来自认证扩展。

真实货币金额由服务器 Catalog（商城目录）与 PurchaseIntent PriceSnapshot（购买意图价格快照）决定；Provider Verify（支付提供器验证）结果必须再与 Order 的 AmountMinor/CurrencyCode（最小单位金额/币种代码）比对。

Receipt Replay（支付凭据重放）通过 ReceiptFingerprint 与 ProviderTransactionId（凭据指纹/支付交易编号）唯一约束阻断；ProviderEventId（支付事件编号）为未来 Webhook（回调）重放保护预留。

Fake Provider（假支付提供器）在 production/shipping（生产/发布）环境初始化失败。Provider Secret、Signing Key、Webhook Secret（提供器密钥/签名密钥/回调密钥）不进入 UE Content、C++ 常量或环境样例明文。

A 玩家查询 B 玩家 Order 会因内部查询绑定 game_id + player_id + order_id（游戏编号+玩家编号+订单编号）而失败。真实支付安全、Webhook 签名和沙箱交易因缺 Production Provider（生产支付提供器）未执行。