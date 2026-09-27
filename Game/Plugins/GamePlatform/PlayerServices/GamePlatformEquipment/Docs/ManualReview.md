# ManualReview（人工审查）

状态：未执行。AI 未代签。

人工审查至少覆盖：三模块构建、Target隔离、0004 Migration、Inventory Ownership、Equip/Unequip、OperationId、两个 Revision、并发 Equip、Equip vs Consume、Outbox、真实 AbilitySet/GameplayEffect Grant、每槽 GrantHandle 撤销、Character-owned/PlayerState-owned ASC Respawn、防重复 Grant、Client StaticMesh/Socket、Remote Observer、Late Join、Dedicated Server + 双客户端、Client/Server Cook、服务重启和跨服恢复。

当前缺 Go/psql/PostgreSQL/UE5.8 运行工具链；AbilitySystem 尚无正式 AbilitySet API；Player↔Character 和 Server↔Player 权威绑定也未落地，因此上述运行项保持未执行。