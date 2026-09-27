# API（接口）

主要公共API：

- FDivineBeastsHeroCatalog（英雄目录）：12个Stable ID、CatalogRevision、Zodiac映射、DisplayNameKey、PrimaryAssetId和异步Definition请求。
- UDivineBeastsHeroDefinition（生肖英雄定义）：平台Definition + ZodiacIdentity/ZodiacTag/ContentPackId/AppearanceSchema。
- UDivineBeastsCharacterComponent（项目角色组件）：可信运行身份绑定、Definition lease、Generation、移动/碰撞配置和Readiness。
- FDivineBeastsCharacterSpawnInitializer（出生初始化适配器）：只初始化已由平台Spawn流程创建的ACharacter。
- IDivineBeastsCharacterCreationProvider（角色创建提供者）：为组合根提供核心Hero Catalog和本地Appearance校验。

没有客户端身份设置RPC、后端HTTP、PlayerData写入或Arena规则接口。
