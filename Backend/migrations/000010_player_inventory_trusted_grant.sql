-- 000010：长期背包可信授予Grant幂等动作；必须在000009迁移之后执行。
-- 不新增客户端API，不赋予公网友好路由发奖能力，也不更改历史Operation记录。
-- 授予和可信消耗时将Inventory.Granted/Inventory.Consumed事实写入000002既有Outbox事务。
-- 回滚策略：已有Grant/Consume操作可能存在时禁止直接撤销类型约束，否则旧审计记录会违反检查。

ALTER TABLE player_inventory_operations
    DROP CONSTRAINT IF EXISTS player_inventory_operations_operation_type_check;

ALTER TABLE player_inventory_operations
    ADD CONSTRAINT player_inventory_operations_operation_type_check
    CHECK (operation_type IN ('Move','Split','Merge','SetQuickbar','ClearQuickbar','Consume','Grant'));

COMMENT ON COLUMN player_inventory_operations.operation_type IS
    '背包持久幂等动作；Grant与Consume限可信游戏服务器事件授权，普通客户端仅允许整理类动作';
