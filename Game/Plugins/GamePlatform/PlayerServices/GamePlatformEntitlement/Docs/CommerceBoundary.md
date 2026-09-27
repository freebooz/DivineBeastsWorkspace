# CommerceBoundary（商业化边界）

本轮不实现 Commerce（商业订单）、支付、钱包或退款支付网关。

未来可信订单可使用 SourceType=CommerceOrder、SourceId=OrderId 产生 Entitlement Grant；可信退款则只 Revoke 对应 Commerce Grant。

如果玩家还拥有 Quest 或其他来源的有效 Grant，撤销 Commerce 来源后权益仍保持有效。