# migrations（PostgreSQL数据库迁移）

该目录是 `Backend（Go业务后端）` 唯一数据库 Migration（迁移）真源，按版本号顺序执行。

当前已包含：

- `000001_core.sql`：玩家长期资料与权威比赛结果；
- `000002_outbox.sql`：Transactional Outbox（事务发件箱）及多副本 Dispatcher 租约字段。

禁止在 `internal/platform/database`、`configs` 或其他目录复制第二套建表脚本；数据库适配器只能消费本目录迁移后的结构。
