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

禁止在 `internal/platform/database`、`configs` 或其他目录复制第二套建表脚本；数据库适配器只能消费本目录迁移后的结构。

`000008_player_inventory.sql`与 `internal/platform/database/postgres/inventory_repository.go`配套。生产启用 Inventory HTTP 链前必须确保该迁移已应用；当前 Runner 没有 psql/PostgreSQL 运行工具链，因此本轮只完成迁移源码与事务仓储实现，未冒充真实数据库迁移已执行。

执行可信消耗操作前还必须应用`000009`迁移，并在PlayerData的服务组合根注入真正验证游戏服身份、服务器实例和玩家绑定的TrustedConsumeAuthorizer（可信消耗授权器）；未注入时一律拒绝。该授权器与物品使用效果的可靠Outbox（事务发件箱）仍需另行实现，不能让客户端或普通Gateway请求绕过。迁移源码已新增，**尚未有实际PostgreSQL执行成功记录**。
