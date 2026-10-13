# migrations（PostgreSQL数据库迁移）

该目录是 `Backend（Go业务后端）` 唯一数据库 Migration（迁移）真源，按版本号顺序执行。

当前已包含：

- `000001_core.sql`：玩家长期 Profile 等核心表；
- `000002_outbox.sql`：Transactional Outbox（事务发件箱）及多副本 Dispatcher 租约字段；
- `000003_session_admission.sql`：Session 准入、实例/预留/绑定和连接代次栅栏；
- `000005_online_identity.sql`：Online 身份、账号与 Session 持久化；
- `000006_online_profile_idempotency.sql`：Profile 更新幂等记录；
- `000007_player_characters.sql`：玩家角色与角色选择幂等记录；
- `000008_player_inventory.sql`：Inventory 聚合根、容器、物品实例、快捷栏和 OperationId 持久幂等结果。
- `000009_player_inventory_trusted_consume.sql`：仅扩展背包幂等操作表允许`Consume`（可信物品消耗），原表、旧版本请求和历史操作均保留；新增功能未暴露公网。
- `000010_player_inventory_trusted_grant.sql`：继续扩展幂等操作类型允许`Grant`（可信授予），需在000009之后执行，保留既有操作和数据。

禁止在 `internal/platform/database`、`configs` 或其他目录复制第二套建表脚本；数据库适配器只能消费本目录迁移后的结构。

`000008_player_inventory.sql`与 `internal/platform/database/postgres/inventory_repository.go`配套。生产启用 Inventory HTTP 链前必须确保该迁移已应用；当前 Runner 没有 psql/PostgreSQL 运行工具链，因此本轮只完成迁移源码与事务仓储实现，未冒充真实数据库迁移已执行。

启用可信授予与消耗必须先应用`000009`、`000010`迁移，并在PlayerData可信组合根注入实际的ServerPlayerVerifier（服务器及玩家归属验证器）、GrantEvidenceSource/ConsumeEvidenceSource（服务端已确认奖励及使用事件）与ItemPolicyCatalog（后端权威物品策略）。缺少任何可信来源默认拒绝，不能从普通Gateway参数获得发奖权限。当前生产事务已将Inventory.Granted/Inventory.Consumed事件同事务写入000002既有Outbox，但游戏服证据源、使用效果消费者、真实生产联调仍未闭环。**尚未有真实PostgreSQL迁移执行成功记录**。详见`Backend/internal/modules/inventory/TrustedGrantAndConsume_20261013.md`。
