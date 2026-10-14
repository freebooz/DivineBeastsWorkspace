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
- `000011_player_inventory_trusted_events.sql`：可信游戏服确认事件持久日志、连接世代绑定与事件不可变性审计。依赖000003准入、000010库存操作和000002事务发件箱。
- `000012_player_inventory_session_lease_renewal.sql`：只对已经正式提交的活跃玩家连接延长最多30秒租约，必须同时核验未撤销的在线账号会话、未禁用账号、实例健康和同Boot/连接/SessionEpoch；不能创建新的准入绑定。

禁止在 `internal/platform/database`、`configs` 或其他目录复制第二套建表脚本；数据库适配器只能消费本目录迁移后的结构。

`000008_player_inventory.sql`与 `internal/platform/database/postgres/inventory_repository.go`配套。生产启用 Inventory HTTP 链前必须确保该迁移已应用；当前 Runner 没有 psql/PostgreSQL 运行工具链，因此本轮只完成迁移源码与事务仓储实现，未冒充真实数据库迁移已执行。

启用可信授予与消耗必须先应用`000009`、`000010`迁移，并在PlayerData可信组合根注入实际的ServerPlayerVerifier（服务器及玩家归属验证器）、GrantEvidenceSource/ConsumeEvidenceSource（服务端已确认奖励及使用事件）与ItemPolicyCatalog（后端权威物品策略）。缺少任何可信来源默认拒绝，不能从普通Gateway参数获得发奖权限。当前生产事务已将Inventory.Granted/Inventory.Consumed事件同事务写入000002既有Outbox，但游戏服证据源、使用效果消费者、真实生产联调仍未闭环。**尚未有真实PostgreSQL迁移执行成功记录**。详见`Backend/internal/modules/inventory/TrustedGrantAndConsume_20261013.md`。

2026-10-13增量：已经新增真实PostgreSQL会话租约验证器、不可变可信事件仓储和独立内部HTTP事件入口。`INVENTORY_TRUSTED_EVENTS_TOKEN`（至少32字节的独立内部强令牌）、`INVENTORY_GAME_ID`（实际后端GameID，例如divine-beasts）和`INVENTORY_POLICY_FILE`（只读规则文件）必须同时配置且相匹配，否则默认不开启可信事件路由。推荐mTLS/服务网格网络白名单进一步限制仅Dedicated Server可访问；普通`NewPlayerDataHandler`和Gateway不注册该路由。相关设计见`Game/Plugins/DivineBeasts/DBAServer/Docs/InventoryTrustedEventIntegration_20261013.md`；未执行真实PostgreSQL迁移和联机验收前不得宣称生产已启用。
2026-10-13会话续租增量：新增 `000012_player_inventory_session_lease_renewal.sql`（已提交真实准入绑定的30秒续租数据库函数），要求在线身份会话未撤销、账号未禁用、服务器保持Ready/健康、当前连接与Boot及SessionEpoch一致。它不创建绑定、不绕过原始 `session_reserve→session_claim→session_commit`（预留→领取→提交）链路，也不复活已经过期的租约。PlayerData生产适配器 `inventory_session_lease_repository.go`（续租仓储）及内部 `POST /internal/v1/playerdata/inventory/session-renew`（服务器会话续租接口）与已有独立Bearer保护共用；公共Gateway不注册。项目Dedicated Server（专用服务器）订阅平台真实准入变更并每10秒尝试续租当前玩家，退出/撤销时精确取消本实例续租。由于现有 `validate-transfer`（迁移票据校验）尚未被核实与 `session_commit` 实际联通，续租只改善**已经正确绑定的会话**，不代表首个进入新手村的玩家已经可以领取物品。上述迁移、Go生产适配和UE代码均待正式编译及实际PostgreSQL验证，严禁将源码存在记录为运行完成。

