# GrantAndRevoke（授予与撤销）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

Grant 输入包含 GrantOperationId、EntitlementId、SourceType、SourceId、StartsAt 与 ExpiresAt。服务端校验 Definition、来源、有效期后，在事务内创建 Grant、提升 Revision、保存 Operation 结果并写 EntitlementGranted Outbox。

Revoke 输入使用 RevokeOperationId，并且只能二选一：指定 GrantId，或指定 EntitlementId + SourceType + SourceId。撤销写入 RevokedAt 与 RevokeReason，不 DELETE Grant 历史。

撤销某一个来源后会重新聚合 Effective Entitlement；只要其他有效 Grant 仍存在，玩家仍拥有该权益。
