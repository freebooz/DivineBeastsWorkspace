# 后续扩展与交接

GamePlatformOpenWorld可消费只读Context、Region和额外Readiness贡献者，添加独立通用开放世界职责；不能重建实例目录或World底座。DivineBeastsOpenWorld负责项目大厅/主城/野外规则与内容，不把这些名字回灌平台。

PCG可用Region/World定义组织生成结果，通过当前代次Provider登记；影响权威玩法的内容仍需服务器权威或一致烘焙。Gameplay/Character可向Loading添加角色/玩法任务，不把Pawn出生塞成World永久必需条件。

GamePlatformServer具备真实身份绑定后，应替换DevelopmentServer装配；客户端消费Session公开Snapshot，补齐身份来源与版本匹配。当前InitializeSessionWorld必须保持失败直到真实适配完成，不能把测试状态内核作为公开连接事实。

项目Bootstrap/World和Foundation世界/区域资产属于开发验证材料，正式产品不得默认启用。后续正式组合根替换开发装配时保留稳定LogicalId策略，资产身份迁移需明确引用与兼容，不批量改名。

续作顺序：解决历史空插件描述决策 → 正式UE三目标构建 → 生成与验证真实地图/定义 → 本地FoundationWorld/回图/重入 → WP真实地图 → Session真实客户端/专服 → 双PIE → 干净Cook/Stage → 人工审查。前置缺失不得用下一插件掩盖。

