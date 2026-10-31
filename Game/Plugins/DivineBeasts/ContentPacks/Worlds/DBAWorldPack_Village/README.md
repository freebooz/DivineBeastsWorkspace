# DBAWorldPack_Village（神兽联盟新手村世界内容包）

2026-10-31资源归属更新：三份硬绑定Village贴图的Production10审核母材质和九份实例归本包`Development/Terrain/Production10`。它们只供开发审核地图，默认不进入正式Client或专服；PluginDemoClient按地图白名单精确放开此开发目录。十张公共V3色彩图归`/DBAPresentationPack_Core/Textures/Terrain/Production10`，项目雪地表现软路径已同步。正式Village共享Landscape母材质、地形、Actor及实例布局未改。历史制作与来源记录保留原路径事实；当前路径合同与阶段证据见`Docs/Implementation/Resources/ResourceOwnershipDelivery_20261031.md`。
## 2026-10-18 新手村地形V3资源最终原生收口（最新进度）

当前正式十层地形BaseColor（基础色）原位覆盖与独立UE冷重载 **10/10已通过**；原Dirt（普通泥土）被锁情况已解决，并无待修改的第十层。中立共享Landscape母材质M_DBA_Village_LandscapeBridge由55节点增量升级为81节点Texture Bombing（纹理轰炸），已有三通道一致UV及高/低画质分支；新建256×256 V3绿色草地共享默认Texture2D并已实际绑定两处VillageGroundAlbedoTexture，独立UE5.8冷重载通过。正式L_Village_Start地图哈希未变，雪/湿润、法线、ORM、PCG、碰撞导航不重新生成，纯客户端高精贴图继续通过资源租约加载。正式服务器共享地图依赖审计通过。

完整UE来源、实物与恢复文档：Docs/Implementation/TerrainColorV3ComplianceReview_20261018.md（十层原位重导入记录）与Docs/Implementation/VillageTerrainTextureBombingV3_20261018.md（绿色默认色与纹理轰炸）。低端与移动端保留普通采样；真实GPU、Client/Server Cook、性能与人工美术验收仍不能宣称通过：新的真实D3D12及GUI跨帧图片依旧黑帧，Server Cook因另一任务持有引擎互斥锁返回NotExecuted。以下9/10段落保留历史，不代表最新状态。

## 2026-10-18 历史阶段（当时9/10，现已由上节10/10完成记录取代）

用户批准直接覆盖后，十层地形稳定BaseColor Texture2D原路径已有9层真实重导入V3色板的派生PNG并在独立UE5.8进程验证；剩余Dirt（泥土）被其他进程文件锁阻断，保留原资产。正式Village地图及中立Landscape母材质SHA未变、原Normal/ORM/Height未变。项目客户端原有VillageSnowWorldSubsystem的草地基础色软路径已更新至V3候选，并配套同一扫描资源的Normal/ORM，保持天气事件和世界异步租约。VillageDevelopmentClient新增Cook与配置，专服三配置已用服务器排除策略生成，不能据此宣称Server Cook已验证。

十二份执行前回退资产及其哈希见Saved/ArtReview/TerrainColorV3FormalCover/Before_20261018/BackupManifest.json；9/10独立冷重载证据与Dirt锁阻断见Docs/Implementation/TerrainColorV3ComplianceReview_20261018.md。实际GPU六态图仍黑帧，真正Client/Server Cook、完整十层和人工最终美术签收未完成，不应标记发布通过。
Dirt安全恢复脚本为Tools/Unreal/Terrain/ResumeTerrainV3FormalCover.ps1，默认只读。其-Apply仅能在本机同工程编辑器安全关闭、纹理文件独占后执行UE原位覆盖与独立十层冷重载，未满足门禁不得提前尝试。



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

## 2026-10-14 外围山峰热带原始森林PCG源点图

- 范围：新手村北、东、西外围山峰，保留湖心岛、桃林、村庄、农田、道路、水体与玩家出生点。
- 项目生态合同：`SourceArt/PCG/TropicalMountainForest_v1.json`，复用 `GamePlatformPCG`（通用PCG机制），不在平台层保存项目雨林资产。
- UE5.8编辑器已实际创建与保存三份原生PCG Graph：`PCG_DBA_Village_OuterTropicalCanopy_Source`（冠层）、`PCG_DBA_Village_OuterTropicalUnderstory_Source`（中层）、`PCG_DBA_Village_OuterTropicalFloor_Source`（林下）。内容归 `Content/PCG/Graphs/`，目前都是有界Landscape候选点图。
- 官方Electric Dreams Environment原始森林资源尚未在本机下载，未迁移正式热带乔木/材质；三图不包含StaticMeshSpawner且未加入正式 `L_Village_Start`，不能宣称新手村森林已经生成或烘焙。
- 源头、许可、空间排除、性能与Cook门禁详见 `Docs/Implementation/GamePlatformPCG/VillageTropicalMountainForest_20261014.md`（外围雨林实施与验收记录）。
## 2026-10-14 Electric Dreams 授权源资产隔离（待正式下载）

- GitHub工程为公开仓库；`Environment/ThirdParty/ElectricDreams/`（受许可第三方原始树木、材质、贴图）只在本地由UE编辑器迁移，按根级`.gitignore`排除公开源代码提交。不复制到GamePlatform（平台层）、MobaCommon（竞技层）或开发审核关卡，也不将第三方源模型发布为单独资产包。
- `Tools/Unreal/PCG/InspectElectricDreamsSample.py`（Electric Dreams官方源工程预检）核对来源项目、森林示例关卡、文件头及公开Git隔离；即使通过也不等同Fab账号授权、UE5.8加载、地图烘焙或Client/Server双端Cook验收。
- 用户需经Fab官方资源库及Epic Games Launcher创建完整示例工程；本地三层PCG候选点资产不依赖不存在的第三方素材。真实迁移后先执行AssetRegistry/依赖闭包审查及命名空间修正，才允许设置正式StaticMeshSpawner（静态网格生成器）。

## 2026-10-16 狼怪物与导航修复

本包持有Quaternius CC0狼的SourceArt、客户端Creatures/Wolf和服务器AI定义。通过Monolith真实导入Idle/Walking、建立平台动画基类派生AnimBP并烘焙出生点外围导航；两个PlayerStart保持原坐标。服务器排除Creatures/Wolf叶子，保留AI与地图。开发配置启用四只狼；发行默认关闭。资源来源与重载证据见Docs/MonolithGenerationManifest.json，实际构建、Cook、人工验收边界见工作空间Docs/Implementation/VillageWolvesAndPartyOperations20261016.md。

## 2026-10-18 月光与薄雾

用户明确要求增加雾、调暗并采用月光照明。已通过Monolith原位保存正式村庄地图：复用既有冷色主光/低强度天光、高度雾，加入三处外围近地LocalFogVolume及固定曝光。未新增Niagara System、材质或运行机制；两个出生点保持原变换。此地图月夜为配色V3的用户指定例外，不修改其他世界。

独立冷重载和实际Client/Server Cook/Stage通过，原生截图归工作空间Saved/Validation/VillageMist_20261018；参数及边界见Docs/Implementation/GamePlatformWorld/VillageMistAndMoonlight_20261018.md。未重启现有联机客户端/服务器，人工可读性与不同画质/GPU仍待验证。
