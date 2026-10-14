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

## 2026-10-12 十层正式地形扫描PBR源的保存与UE验收边界

2026-10-13增量：用户要求新手村草地，已通过Monolith将共享Landscape母材质增补为55节点，增加250厘米重复的地面Albedo/Normal/ORM参数；默认纹理仍只引用本包三张中性Texture2D。对应高清扫描草地三贴图由第三层`DBAPresentationPack_Core/Textures/Terrain/Grass`持有，客户端Data世界租约写入MID，不给地图硬引用纯表现包。本轮不写地图、不修改两个PlayerStart、地形高度/碰撞/导航，也不表示十层地形全面接入。新C++构建、真实PIE绿色回归与新Cook发布仍待完成，不能以材质保存替代运行验收。

- 十种地表原`SourceArt/Terrain/PBR/*`技术纹理已按用户明确授权由同名正式CC0扫描2K BaseColor、DirectX Normal、ORM替换，另为每类增补微Height；原30张旧PNG与30个旧Texture2D UE二进制已保存到独立61项回退包。正式来源见`Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json`；原`TerrainSourceManifest_v1.json`只作历史地貌校准证据。
- 源图成功不意味着本包的真实UE Texture2D已重导入。独立审核用高精多层母材质与表现实例归`DBAPresentationPack_Core/Materials/Terrain`（客户端纯表现），不直接写正式`L_Village_Start`、`M_DBA_Village_LandscapeBridge`或服务器权威世界定义。
- 需等主工程Editor完成新的DivineBeastsDemoClient模块编译及并行UBT释放，再运行`RunTerrainGaeaFullReview.ps1 -Apply -ProductionPBR`，验证40个正式UE Texture2D、10层Landscape/PCGGraph、雪苔湿润及客户端/专服容器。因共享本包仍可能被Server AlwaysCook扫描，禁止在没有独立专服Cook/UnrealPak审查前宣称高清扫描Texture2D已从服务器剥离。
- 实施记录：`Docs/Implementation/TerrainProductionPBRIntegration_20261012.md`。2026-10-11已验证的正式Village雪层桥接/联机不因本次源图替换失效，也不代表新正式十层已生效。

## 2026-10-11 正式Village写实积雪接入（共享地图与客户端隔离）

- 真正通过UE5.8编辑器制作并绑定了 `/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge`（新手村共享地形母材质，48个有效表达式）和三张本内容包持有的低容量中性默认Texture2D。正式 `L_Village_Start` 的唯一Landscape已保存该材质并开启 `bUseDynamicMaterialInstance`（动态材质实例），不使用EditorOnly的运行时换材质接口。
- `DivineBeastsPresentationClient`（神兽联盟第三层客户端表现模块）的 `UDivineBeastsVillageSnowWorldSubsystem`（新手村雪层世界子系统）根据世界身份订阅已有 `GamePlatformWeatherClient`（天气客户端状态），使用 `GamePlatformData` 的World级异步租约加载 `GamePlatformSurface` 的高清雪覆盖/基础色/法线贴图，在Landscape MID写入雪量、湿润度及三个Texture参数；销毁时释放所有租约、弱引用与订阅。不为服务器创建第二套天气权威或雪粒子复制。
- 正式共享地图和母材质经过UE AssetRegistry依赖校验，未硬引用`GamePlatformSurface`、`GamePlatformVFX`、`DBAPresentationPack_Core`纯客户端内容。客户端使用VillageDevelopmentClient发布配置显式Cook高清资源，DedicatedServer Cook只持有WorldPack的中性母材质和三张共享基础贴图。
- 当前代码已修复运行时材质完整路径识别错误，并对真实Landscape MID的雪量/湿润和三张纹理执行回读门禁：`VILLAGE_SNOW_MID_VERIFY_SUCCESS`代表对应世界的材质实际绑定成功；贴图已验证后天气过渡只更新标量，避免重复绑定纹理。
- 这些是**视觉装配与运行时资源绑定**，不得据此宣称玩家看到大雪、近地雪粉无穿透、双客户端网络同步及GPU预算均通过；人工作业仍需实际大雪场景检查。详见 `Docs/Implementation/VillageSnowBridgeRuntimeClosure_20261011.md`。
