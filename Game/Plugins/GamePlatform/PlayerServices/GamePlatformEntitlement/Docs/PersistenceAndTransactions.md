# PersistenceAndTransactions（持久化与事务）

正式 Migration 为 `0003_player_entitlement`，建立 player_entitlement_state、player_entitlement_grants 和 player_entitlement_operations。

Grant/Revoke 先锁定玩家 state 行，再执行历史变更、Revision++、保存 Operation Result、写 Outbox，最后 Commit。失败在同一事务回滚。

并发不同 OperationId 的 Grant/Revoke 会在 `FOR UPDATE` state 行上串行化，使 Revision 单调连续。