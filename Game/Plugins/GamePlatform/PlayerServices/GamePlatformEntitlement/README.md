# GamePlatformEntitlement（游戏平台权益插件）

正式路径：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformEntitlement`。

UE（虚幻引擎）侧由两个模块组成：`GamePlatformEntitlement（权益共享运行时模块）`为 Runtime（双端运行时），提供 Definition（定义）、Snapshot（快照）、查询和服务器安全只读契约；`GamePlatformEntitlementClient（权益客户端模块）`为 ClientOnly（仅客户端），负责 LocalPlayer（本地玩家）快照缓存、Gateway（网关）读取、账号切换隔离和 Hero/Skin（英雄/皮肤）显示投影。

长期权益权威位于现有 `PlayerDataService（玩家数据服务） + PostgreSQL（关系数据库）`。Grant（授予）/Revoke（撤销）保存完整审计历史，多来源 Grant 聚合成 Effective Entitlement（有效权益），有效期按后端可信 UTC 查询时计算。没有创建 Entitlement 独立微服务，也没有将 Inventory Item（背包物品）当作 Hero/Skin 权益。

当前源码已覆盖 Shared/Client、Entitlement Go 领域、0003 Migration（数据库迁移）、Repository（仓储）、OperationId（操作编号）持久幂等、Revision（修订号）、Outbox（事务外发）、Gateway/PlayerData API、Quest Reward（任务奖励）稳定 Grant、Dedicated Server（专用服务器）Hero/Skin 授权检查边界。

当前 Runner（运行器）缺少 Go/psql/PostgreSQL/UE5.8（虚幻引擎5.8）运行工具链，正式 PlayerAuthenticator（玩家认证器）、ServerAssignment（服务器分配绑定）验证器和消息订阅器也尚未实现，因此真实登录、数据库事务运行、并发、Quest Reward 消费、Hero/Skin 真流程、Build/Cook（构建/烘焙）和人工审查均保持“未执行”。
