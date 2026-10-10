# DBAServer（神兽联盟服务器插件）

正式位置：`Game/Plugins/DivineBeasts/DBAServer/`。本插件承载项目服务器组合模块`DBAServer`；`DivineBeastsArenaServer`（MainArena项目扩展）已归可选DBAArena，本插件不强制依赖竞技。

2026-10-10角色准入修复：Village桥接必须显式部署`PLAYERDATA_BASE_URL`，指向受保护内网中的既有PlayerData HTTP服务（本地验证为`http://127.0.0.1:18082`）。专用服务器先用当前真实准入的PlayerId读取`/internal/v1/playerdata/profile`及`characters`，校验已选角色属于该玩家、状态Active和核心Hero Catalog，再提交平台出生门禁。每个当前连接冻结一次资料选择快照，不接受客户端Hero自报；Pawn生成事实事件绑定可信CharacterId/Hero及出生、Avatar代次，公开Hero复制给全部观察者，持久CharacterId继续仅Owner复制。

内部PlayerData当前HTTP端口沿用后端既有可信网络边界，本修复不把其暴露到公网，也不新增或扩大访问权限。地址缺失、HTTP失败、错玩家/错角色、重复身份、已撤销连接和旧世界回调均失败关闭，不回退为固定英雄。请求超时5秒、响应最多2MiB，服务器世界关闭时取消自有请求和订阅；资料响应、账号身份及环境凭据不进入客户端或日志。服务器须实际Cook`/DBAGameplay/Definitions`的逻辑定义，并独立审计客户端Mesh/动画剥离。

身份受理后仍必须消费真实角色Ready。桥接订阅必要定义的成功/失败/取消事实并设置10秒初始化截止；失败或已Ready后资源失活走下一游戏线程时隙的精确准入释放。平台`ValidatePlayerActivation`在消费准备令牌之前重新验证真实准入和项目角色Ready；默认实现继续调用准入验证，不凭客户端布尔值放行。客户端等当前Pawn的Ready事件后才提交一次Prepare令牌。重生时更换Pawn操作身份并取消旧计时器，旧超时或延迟失败不能撤销后继Avatar。

`DBAServer` 从 `Deploy/Server/<ServerRole>/server-profile.json` 读取角色、体验、地图和必要资源清单；必须同时匹配Shared角色目录、Profile允许体验及实际启动世界，才能向通用`GamePlatformServer`请求注册并发布Ready。正式启动参数为`-ServerRole=OpenWorld|Village|MainArena`，可选`-ExperienceId=<体验ID>`覆盖默认体验，但只能选择Profile允许的体验。大厅使用OpenWorld角色，其默认体验为`Experience.OpenWorld.Hub`；`Experience.Lobby.Main`只保留兼容映射，不对应单独角色或Profile。

服务器实例ID、区域、世界ID、端点、容量和版本来自受控环境变量；控制面地址与内部令牌只由`GamePlatformServer`在发起请求时读取环境变量，不写入Profile。`DivineBeastsArenaServer`继续独立负责MainArena竞技服务扩展，不与通用DBAServer组合模块互相依赖。

Ready门禁要求实际地图匹配Profile，且每个必需软引用对象已加载。新手村现有真实`/DBAWorldPack_Village/Maps/L_Village_Start`及三种世界定义；其他角色地图和完整注册／准入仍需按实际产物与环境分别验证，不能以Profile声明代替资源或运行证据。模块不会伪造地图或在模块启动时自动载图。迁移前文档和旧描述快照见`Docs/Legacy/`。

唯一`DivineBeastsArenaServer`目标通过`CustomConfig=DedicatedServer`把专用服务器发布规则身份写入构建收据。UAT打包阶段据此读取`Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini`，排除DBAClient、公共UI／前端、VFX与Surface客户端目录。`-AdditionalCookerOptions=-CustomConfig=VillageServer`只选择新手村烘焙配置，不能替代打包阶段的收据配置。此规则保持三角色共用程序，客户端目标不使用该规则；最终剥离结论必须核验实际发布包并运行专用服务器。

2026-10-09补充：Village Ready还必须等待唯一GamePlatformGameplay体验进入真实Active。控制面注册和体验激活完成顺序不固定，均进入TryPublishReady同一门禁；PublishingReady不重复提交。体验失败/超时清除启动计时器、停止心跳并请求排空，迟到的注册/Ready回调不会覆盖项目Failed终态。其他两角色仍沿用既有Profile资源门禁，未在本轮提升其运行验收结论。当前Village已用独立新Cook包实测Active并持续心跳；客户端WorldReady与行走仍须单独验证。
