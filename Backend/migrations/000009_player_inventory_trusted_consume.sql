-- 000009：扩大长期背包幂等事务可记录的可信服务器物品消耗类型。
-- 旧Move/Split/Merge/Quickbar记录及其OperationId原样保留；不删除任何库存或账本。
-- 新增Consume只允许可信服务经游戏服身份与玩家绑定校验后调用，公网Gateway无此路由。
-- 本迁移不授予任何物品，也不作为任务奖励或游戏效果提交成功的证据。

ALTER TABLE player_inventory_operations
    DROP CONSTRAINT IF EXISTS player_inventory_operations_operation_type_check;

ALTER TABLE player_inventory_operations
    ADD CONSTRAINT player_inventory_operations_operation_type_check
    CHECK (operation_type IN ('Move','Split','Merge','SetQuickbar','ClearQuickbar','Consume'));

COMMENT ON COLUMN player_inventory_operations.operation_type IS
    '背包幂等业务动作；Consume仅用于受控可信服务消耗，禁止普通客户端直接提交';
