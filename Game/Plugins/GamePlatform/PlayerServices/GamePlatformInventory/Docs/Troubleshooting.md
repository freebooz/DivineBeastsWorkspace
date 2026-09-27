# Troubleshooting（故障排查）

客户端 InventoryNotLoaded：确认 Online/Identity 已获得真实 AccessToken，创建 `FGamePlatformInventoryGatewayHttpTransport（背包网关HTTP传输）`并调用 ConfigureAuthenticatedAccount（配置认证账号）。Gateway 默认 Run 在无认证器时会故意失败。

RevisionConflict：进入 Reconcile（对账）刷新 Snapshot；不要自动生成新 OperationId 无限重试。Backend/网络错误且 Pending 仍存在时，RetryPendingOperation（重试待处理操作）复用原 OperationId。

Grant DefinitionNotFound：检查 `INVENTORY_ITEM_POLICY_FILE`是否包含受控 ItemDefinitionId。InventoryFull：检查 max_slots 与已占用槽；事务失败不会保留半发奖。

Pickup 长时间 Reservation（保留）：说明 Grant OutcomeUnknown 后的 Operation 查询仍无确定结果。不要再次发起新 Grant；应使用原 OperationId 查询 PlayerData 后再 Finalize/Cancel。
