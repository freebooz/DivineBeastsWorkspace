# Architecture（架构）

`GamePlatformEntitlement（游戏平台权益插件）`位于 GameFoundation/PlayerServices（游戏平台基础层/玩家服务分类），包含 Runtime 双端共享模块和 ClientOnly 客户端模块。

共享模块只负责稳定 Entitlement 类型、Definition、Snapshot 与查询，不持有 HTTP、数据库或客户端凭据；客户端模块通过 Gateway 获取当前认证账号的有效权益 Snapshot。长期权威在 PlayerDataService + PostgreSQL。

可信 Dedicated Server 通过 DBAServer（神兽联盟服务端组合层）的 Entitlement 适配器向 PlayerData 发起授权检查。DBAServer 只依赖共享 `GamePlatformEntitlement`，禁止依赖 `GamePlatformEntitlementClient`。

后端继续使用既有 PlayerDataService，不增加 EntitlementService（权益微服务）。