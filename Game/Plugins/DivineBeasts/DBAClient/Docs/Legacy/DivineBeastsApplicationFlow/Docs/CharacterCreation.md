# CharacterCreation（角色创建）

CreatePersistentCharacter 使用 OperationId（操作编号）保证幂等。客户端提交 HeroDefinitionId、CharacterName、AppearanceSelection、ExpectedCatalogRevision、ExpectedContractVersion；不提交 Eligible=true 等权威判断。

PlayerDataService 根据认证 PlayerId 重新校验请求、Hero资格和契约版本；PostgreSQL 使用 divinebeasts_character_create_operations 的 (game_id, player_id, operation_id) 唯一键和 request_hash 防重复/冲突。

角色档案不保存 CurrentHP、Buff 或技能冷却。
