# BackendDomain（后端领域）

Inventory 公共领域位于 `Backend/gameplatform/inventory（平台背包领域）`，包含 ItemInstance/Snapshot/Command（物品实例/快照/命令）、错误体系、PolicyCatalog（物品策略目录）、Repository Port（仓储端口）、Service（应用服务）与 QuestRewardHandler（任务奖励处理器）。

PostgreSQL 适配位于 `Backend/gameplatform/internal/persistence/inventoryrepository（内部背包仓储）`。PlayerDataService（玩家数据服务）承载 Inventory；没有新增 `cmd/InventoryService（独立背包服务）`。

Gateway 公共源码位于 `Backend/gameplatform/gateway（平台网关）`。`PlayerAuthenticator（玩家认证器）`必须来自正式 Identity/Online（身份/在线）链。默认 Gateway `Run（运行）`在认证器未配置时明确失败，不能以客户端自报 PlayerId 替代认证。
