# BackendDomain（后端领域）

公共 Equipment 领域位于 `Backend/gameplatform/equipment`，包含模型、错误、DefinitionCatalog（定义目录）、Repository Port（仓储端口）、Service（应用服务）和 Go 测试源码。

PostgreSQL 实现位于 `Backend/gameplatform/internal/persistence/equipmentrepository`。PlayerDataService 负责装配 Equipment Repository/Service/HTTP Handler。

没有创建 EquipmentService（装备独立微服务）。CharacterId 字段已保留，但当前仓库没有真实角色主数据/归属表，因此 PlayerId↔CharacterId 权威归属校验仍未执行。