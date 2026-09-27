# PerformanceAndScalability（性能与扩展）

Catalog（商城目录）是低频配置，Client 按 CatalogRevision（目录修订号）缓存，不随 UI 刷新重复下载。PurchaseIntent/Order（购买意图/订单）是用户触发路径，不做 Tick（每帧）请求。

数据库对 player/order、player/offer/period、provider transaction/receipt、fulfillment operation（玩家/订单、玩家/报价/周期、支付交易/凭据、履约操作）建立索引或唯一键。购买限制检查使用每玩家每 Offer 的 advisory transaction lock（事务咨询锁），避免并发超买。

Client 本地 Debounce（防重复点击）只改善交互，真正幂等由 RequestId、PurchaseIntentId、OrderId（请求编号、购买意图编号、订单编号）和数据库约束保证。

当前无真实吞吐压测数据，不承诺 QPS（每秒请求数）。后续应记录 Catalog bytes、Intent latency、Provider latency、Payment verify、Fulfillment step、DB transaction 与 Gateway latency（目录字节数、意图延迟、支付提供器延迟、支付验证、履约步骤、数据库事务、网关延迟）。