# PurchaseIntent（购买意图）

客户端点击购买首先创建 PurchaseIntent。输入只允许 OfferId、Quantity、RequestId。

后端解析当前 Product/Offer/Price/RewardSet（商品/报价/价格/奖励集），重新校验 Offer Active、LiveOps Eligibility、Reward Target 和购买数量，生成不可变的 PriceSnapshot 与 RewardSnapshot。

PurchaseIntentId 由 game/player/request 稳定生成，`(game_id,player_id,request_id)`数据库唯一，重复 RequestId 返回同一意图。

TTL（有效期）由 `COMMERCE_INTENT_TTL_SECONDS（商城购买意图有效秒数）`配置，范围 60～3600 秒，默认 600 秒。Intent 过期后必须重新创建并重新验证价格/Offer。