# SSLM背包可信拾取、任务奖励及使用事务（2026-10-13）

## 已写入源码

- `inventory/grant.go`（可信授予）：`GrantOperationID`由玩家和已确认来源事件确定性派生，`GrantTrusted`默认拒绝未授权来源。`PlanTrustedGrant`先填充同物品定义和同权威堆叠上限的活跃实例，后按容器与槽位升序分配；背包放不下时全部拒绝，不部分发奖。
- `grant_authorizer.go`（可信奖励授权器）：必须同时确认`ServerPlayerVerifier`（当前游戏服启动代次及玩家归属）、`GrantEvidenceSource`（服务端已确认奖励事实）与`ItemPolicyCatalog`（后端权威物品规则）。单靠请求填入的服务器/玩家身份不能授权。
- `consume_authorizer.go`（可信消耗验证器）：验证已确认的使用事件、玩家、物品实例、数量、服务器Boot及物品可消费性，同事件操作号固定，不能二次扣除。
- `policy_file.go`及`Backend/configs/inventory_item_policies_v1.json`：严格JSON导入，首批8类桃源村物品、权威堆叠上限、可消费属性和允许的授予来源。配置归现有正式 `configs/`，不另建平行配置根；客户端图标/中文说明不参与权限。
- `postgres/inventory_grant_repository.go`（PostgreSQL可信授予事务）：同事务提交堆叠、新物品实例、修订号、OperationId幂等记录和`Inventory.Granted` Outbox。`inventory_repository.go`的可信消耗也在同事务写入`Inventory.Consumed`事件；完全消耗时依外键级联清除快捷栏。
- `000010_player_inventory_trusted_grant.sql`：在`000009`之上扩大Operation类型以容纳Grant；没有新增客户端发奖API。

## 安全与验收边界

真实专用服务器玩家会话验证、世界掉落/任务奖励事实生产者、使用效果消费端及其幂等、防复制、多实例联机接线尚未接入组合根；当前`NewService`（普通背包服务）拒绝Grant和Consume。不能把内部领域方法存在解读为玩家已经可以联机拾取或使用物品。

迁移应按`000002`、`000008`、`000009`、`000010`顺序应用。操作幂等记录与Outbox必须使用同一PG事务；跨服务NATS订阅方也需自行按消息ID去重。未取得数据库实际执行证据前，不能宣布生产环境已迁移。

新增`grant_test.go`、`grant_authorizer_test.go`和`consume_authorizer_test.go`（Go领域自动化测试）覆盖可信授予、服务器身份拒绝、确定性操作号、原子满包拒绝、跨容器分配和消耗幂等；实际测试结果单独记录。
## 2026-10-13 增量：背包授权容器限制与图标异步加载

- `GrantRule.AllowedContainerIDs`（奖励允许容器清单）属于服务器策略，不允许客户端指定。未配置时仅授权 `main`（随身背包），主背包满则原子拒绝，不得把掉落奖励自动转存仓库。`ItemPolicy.AllowedGrantContainers`（权威规则目录的可选容器清单）可以显式授权其他容器，最多8个且不允许重复/无效标识；授权清单参与可信授予幂等指纹。
- `DivineBeastsInventoryScreen`（神兽联盟背包页面）新增 `RenderInventoryIcon`（按槽位显示图标）、`RequestInventoryIcon`（异步图标加载）和 `CancelInventoryIconLoads`（退出页面时取消），继续使用已存在的 `FGamePlatformAssetLoader`（平台软资源加载器）与资产软路径。仅检测到真实 `SlotIcon_0..31`（32格图标）或 `QuickIcon_0..11`（12快捷格图标）才请求纹理，缺少控件/纹理时只显示中文物品名与数量。页面解绑清理句柄，异步完成验证页面代次，不使用 Tick（逐帧轮询）或同步读取纹理。
- 8张正式 Texture2D（图标纹理）、8个 DataAsset（显示定义数据资产）及Widget树内图标控件依旧需要 Monolith 在真正的虚幻编辑器中制作。此文档不表示图标资产已经存在或当前源码通过新的UE编译。

