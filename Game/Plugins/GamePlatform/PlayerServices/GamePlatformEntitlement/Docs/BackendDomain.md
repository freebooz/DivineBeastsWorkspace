# BackendDomain（后端领域）

公共 Entitlement 领域位于 `Backend/gameplatform/entitlement`，包含模型、错误、DefinitionCatalog、Repository Port、Service、Quest Reward Handler 和测试源码。

PostgreSQL 适配位于 `Backend/gameplatform/internal/persistence/entitlementrepository`。PlayerDataService 装配 Repository/Service/HTTP Handler，Gateway 只代理当前认证玩家的 Snapshot。

没有创建 `cmd/EntitlementService` 或 `cmd/entitlementservice`。