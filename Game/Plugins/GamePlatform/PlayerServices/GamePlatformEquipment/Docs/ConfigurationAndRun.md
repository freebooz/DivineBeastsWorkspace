# ConfigurationAndRun（配置与运行）

PlayerDataService 新增必需环境变量 `EQUIPMENT_DEFINITION_FILE（装备定义文件）`。本地样例指向 `Backend/configs/games/divinebeasts.equipment.development.json`，生产样例要求挂载正式发布目录。

DBAServer Equipment Adapter 复用 `PLAYERDATA_BASE_URL`、`PLAYERDATA_INTERNAL_TOKEN`、`GAME_SERVER_INSTANCE_ID`和`GAME_ID`。

当前 Development Catalog 只有文本规则，没有伪造 DA_Equipment_FoundationSword 或 Mesh 资产；真正 DataAsset/Socket 必须由 Unreal Editor（虚幻编辑器）创建和验证。