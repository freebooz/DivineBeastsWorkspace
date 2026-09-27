# ManualReview（人工审查）

状态：未执行。AI 未代签。

人工审查需在真实环境覆盖：ClientOnly 模块隔离、Server Target不链接InventoryClient、Gateway真实登录与玩家隔离、普通Client不能Grant、专服Assignment绑定、0002 Migration、Move/Swap/Split/Merge/Quickbar、Grant/Consume幂等、事务回滚、并发Move/Grant、Quest Reward一次性、Pickup双客户端竞争和OutcomeUnknown恢复、0/100/500 Item规模、Client Cook、Server Build、Outbox发布恢复。

当前缺少 Go/psql/PostgreSQL/UE5.8完整运行环境及正式 PlayerAuthenticator、ServerAssignment验证器、NATS Publisher，因此这些运行项保持未执行。
