# DBAWorldPack_Village（神兽联盟新手村世界内容包）

本内容包是神兽联盟第三层正式 Village（新手村）世界资产所有者，只承载真实世界内容，不实现平台机制或项目运行时代码。

当前首批交付：

- `/DBAWorldPack_Village/Maps/L_Village_Start`：新玩家首次进入的正式新手村起始地图。
- `/DBAWorldPack_Village/Definitions/DA_DBA_World_Village_Tutorial`：`Experience.Village.Tutorial（新手教学体验）` 对应的 `UDivineBeastsWorldDefinition（神兽联盟世界定义）`。
- `/DBAWorldPack_Village/Definitions/DA_DBA_Experience_Village_Tutorial`：唯一教学体验装配定义，Purpose必须为非空用途；缺失时运行期Data验证拒绝，不能发布服务器Ready。
- `/DBAWorldPack_Village/Definitions/DA_DBA_Pawn_WorldCharacter`：双方使用的原生共享世界角色定义，真实Pawn类由DBAGameplay持有。
- 后续 `Village.Main（主新手村）`、`Village.Training（训练体验）` 可复用同一地图或按正式内容需求增加地图，但必须继续由本内容包持有。

边界要求：

- `GamePlatformWorld（游戏平台世界插件）`、`GamePlatformPCG（游戏平台程序化内容生成插件）`、`GamePlatformSurface（游戏平台环境表面材质插件）`继续持有通用机制。
- `DBAWorlds（神兽联盟项目世界插件）`只持有项目世界定义类型与校验。
- 本包不创建C++模块，不复制平台世界逻辑；Client、Dedicated Server、Editor均需挂载本包以读取同一权威地图身份。
- 纯客户端VFX/Surface表现资源不得成为服务器就绪的硬依赖；碰撞、导航、权威PCG结果可随地图进入Server Cook（服务器烘焙）。
