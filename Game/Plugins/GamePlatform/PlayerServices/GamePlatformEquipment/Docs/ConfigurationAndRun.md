# ConfigurationAndRun（配置与运行）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

PlayerDataService 新增必需环境变量 `EQUIPMENT_DEFINITION_FILE（装备定义文件）`。本地样例指向 `Backend/configs/games/divinebeasts.equipment.development.json`，生产样例要求挂载正式发布目录。

DBAServer Equipment Adapter 复用 `PLAYERDATA_BASE_URL`、`PLAYERDATA_INTERNAL_TOKEN`、`GAME_SERVER_INSTANCE_ID`和`GAME_ID`。

当前 Development Catalog 只有文本规则，没有伪造 DA_Equipment_FoundationSword 或 Mesh 资产；真正 DataAsset/Socket 必须由 Unreal Editor（虚幻编辑器）创建和验证。
