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

## 2026-10-11 正式Village写实积雪接入（共享地图与客户端隔离）

- 真正通过UE5.8编辑器制作并绑定了 `/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge`（新手村共享地形母材质，48个有效表达式）和三张本内容包持有的低容量中性默认Texture2D。正式 `L_Village_Start` 的唯一Landscape已保存该材质并开启 `bUseDynamicMaterialInstance`（动态材质实例），不使用EditorOnly的运行时换材质接口。
- `DivineBeastsPresentationClient`（神兽联盟第三层客户端表现模块）的 `UDivineBeastsVillageSnowWorldSubsystem`（新手村雪层世界子系统）根据世界身份订阅已有 `GamePlatformWeatherClient`（天气客户端状态），使用 `GamePlatformData` 的World级异步租约加载 `GamePlatformSurface` 的高清雪覆盖/基础色/法线贴图，在Landscape MID写入雪量、湿润度及三个Texture参数；销毁时释放所有租约、弱引用与订阅。不为服务器创建第二套天气权威或雪粒子复制。
- 正式共享地图和母材质经过UE AssetRegistry依赖校验，未硬引用`GamePlatformSurface`、`GamePlatformVFX`、`DBAPresentationPack_Core`纯客户端内容。客户端使用VillageDevelopmentClient发布配置显式Cook高清资源，DedicatedServer Cook只持有WorldPack的中性母材质和三张共享基础贴图。
- 当前代码已修复运行时材质完整路径识别错误，并对真实Landscape MID的雪量/湿润和三张纹理执行回读门禁：`VILLAGE_SNOW_MID_VERIFY_SUCCESS`代表对应世界的材质实际绑定成功；贴图已验证后天气过渡只更新标量，避免重复绑定纹理。
- 这些是**视觉装配与运行时资源绑定**，不得据此宣称玩家看到大雪、近地雪粉无穿透、双客户端网络同步及GPU预算均通过；人工作业仍需实际大雪场景检查。详见 `Docs/Implementation/VillageSnowBridgeRuntimeClosure_20261011.md`。
