# DBAServer（神兽联盟服务器插件）

正式位置：`Game/Plugins/DivineBeasts/DBAServer/`。本插件承载项目服务器组合模块`DBAServer`；`DivineBeastsArenaServer`（MainArena项目扩展）已归可选DBAArena，本插件不强制依赖竞技。

`DBAServer` 从 `Deploy/Server/<ServerRole>/server-profile.json` 读取角色、体验、地图和必要资源清单；必须同时匹配Shared角色目录、Profile允许体验及实际启动世界，才能向通用`GamePlatformServer`请求注册并发布Ready。正式启动参数为`-ServerRole=OpenWorld|Village|MainArena`，可选`-ExperienceId=<体验ID>`覆盖默认体验，但只能选择Profile允许的体验。大厅使用OpenWorld角色，其默认体验为`Experience.OpenWorld.Hub`；`Experience.Lobby.Main`只保留兼容映射，不对应单独角色或Profile。

服务器实例ID、区域、世界ID、端点、容量和版本来自受控环境变量；控制面地址与内部令牌只由`GamePlatformServer`在发起请求时读取环境变量，不写入Profile。`DivineBeastsArenaServer`继续独立负责MainArena竞技服务扩展，不与通用DBAServer组合模块互相依赖。

Ready门禁要求实际地图匹配Profile，且每个必需软引用对象已加载。新手村现有真实`/DBAWorldPack_Village/Maps/L_Village_Start`及三种世界定义；其他角色地图和完整注册／准入仍需按实际产物与环境分别验证，不能以Profile声明代替资源或运行证据。模块不会伪造地图或在模块启动时自动载图。迁移前文档和旧描述快照见`Docs/Legacy/`。

唯一`DivineBeastsArenaServer`目标通过`CustomConfig=DedicatedServer`把专用服务器发布规则身份写入构建收据。UAT打包阶段据此读取`Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini`，排除DBAClient、公共UI／前端、VFX与Surface客户端目录。`-AdditionalCookerOptions=-CustomConfig=VillageServer`只选择新手村烘焙配置，不能替代打包阶段的收据配置。此规则保持三角色共用程序，客户端目标不使用该规则；最终剥离结论必须核验实际发布包并运行专用服务器。
