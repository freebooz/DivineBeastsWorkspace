# PersistenceAndTransactions（持久化与事务）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

正式 Migration 为 `0003_player_entitlement`，建立 player_entitlement_state、player_entitlement_grants 和 player_entitlement_operations。

Grant/Revoke 先锁定玩家 state 行，再执行历史变更、Revision++、保存 Operation Result、写 Outbox，最后 Commit。失败在同一事务回滚。

并发不同 OperationId 的 Grant/Revoke 会在 `FOR UPDATE` state 行上串行化，使 Revision 单调连续。
