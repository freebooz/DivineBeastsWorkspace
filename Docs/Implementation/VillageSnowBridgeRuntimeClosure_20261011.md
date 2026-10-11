# 神兽联盟正式Village写实积雪技术闭环与人工验收边界（2026-10-11）

## 1. 本轮交付结论

已完成**正式新手村地形材质、客户端积雪软资源装配、真实运行时Landscape MID参数回读、完整客户端和服务器Development构建、双端真实Cook/Stage/IoStore及容器资源隔离验证**。此次将上一轮遗留的正式地图无LandscapeMaterial阻断修复为可运行、可复核的技术交付。

**不代表全部完工**：尚无用户亲眼见到的大雪/小雪/晴天连续切换截图、贴地雪粉碰撞/穿地审核、GPU Profile实测、双客户端+专用服务器天气过渡/断线重连综合验证。人工最终验收保持“待签收”，没有以自动化结果代签。

## 2. 三层职责和新增实际资产

第一层GamePlatformWeather（平台天气）仍拥有唯一网络权威与客户端雪量事实，GamePlatformSurface（通用表面）保存高清雪覆盖、基础色、法线及其原有MPC，GamePlatformVFX（通用特效）保存NS_GP_Weather_Snow_Detailed近景、远景、贴地风吹雪粉和天气特效定义。第二层MobaCommon没有特殊职责，没有新增模块或向下反向依赖。

第三层DBAWorldPack_Village（正式新手村世界内容包）现已在UE5.8真实编辑器下生成并保存：
- 地形母材质 `/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge`，48个有效节点；
- 三张小尺寸中性地形默认Texture2D（Coverage、Albedo、Normal），严格由WorldPack自身持有；
- 正式 `/DBAWorldPack_Village/Maps/L_Village_Start` 唯一Landscape保存 `landscape_material` 并启用 `use_dynamic_material_instance`；
- 原生UE AssetRegistry只读审计：地图包8项引用、母材质3项引用，禁止的纯客户端Surface/VFX/DBAPresentation硬依赖0项。

第三层DBAClient/DivineBeastsPresentationClient（新手村项目客户端表现模块）新增 `UDivineBeastsVillageSnowSettings`（高清纹理软路径配置）和 `UDivineBeastsVillageSnowWorldSubsystem`（客户端世界生命周期装配），依赖既有GamePlatformData资源租约及WeatherClient事件，只有真实Village/Game/PIE且不是DedicatedServer时运行。不新增服务器Gameplay状态或网络雪片RPC。

## 3. 真实运行逻辑修复

- 初版真实客户端启动后曾报告“中性雪层母材质不匹配”，原因已定位为代码中的 `ExpectedBridgePath` 少了 `Landscape/` 子目录；实际上打包地图加载的材质路径正确。已修复。
- 独立Client Development构建后，按新VillageDevelopmentClient配置重新Cook/Stage，实际进入 `L_Village_Start` 并获得GamePlatformData异步加载三张高清雪纹理成功。
- 运行时通过 `UMaterialInstanceDynamic::K2_GetScalarParameterValue` 与 `K2_GetTextureParameterValue` 回读验证SnowAmount、Wetness和三张高清雪纹理均与写入值匹配，只有所有MID同时验证才写日志 `VILLAGE_SNOW_MID_VERIFY_SUCCESS`。最新日志：`D:/SnowValidation20261011/VillageBridgeFinalClient20261011/VillageSnowFinalRuntimeSmoke.log`，确认一处正式Landscape通过。
- 按事件驱动更新，只在新Landscape或首次成功绑定时设置高分辨率Texture；使用 `TSet<TWeakObjectPtr<ALandscapeProxy>>` 记录世界范围的已绑定表面，后续天气过渡只同步标量、不按每0.1秒重新绑定三张Texture。世界销毁时释放弱集合、事件订阅及Data租约，无持续Tick。
- 当前该性能优化对Village单Landscape经过实际运行回读验证；大型World Partition流送中已有Landscape组件重建、材质MID再生成的情况须单独验收，不能将单场景结果泛化。

## 4. 实际编译、Cook、Stage与资源隔离

- Client正式目标：`Build.bat DivineBeastsArenaClient Win64 Development` 完整构建、链接、WriteMetadata真实成功，退出码0；最后一次包含弱集合优化与MID回读检查。
- Server正式目标：`Build.bat DivineBeastsArenaServer Win64 Development` 最新可用构建检查退出码0，服务器未增加任何客户端源码依赖。
- 编辑器侧的真实UE Landscape创建与回读：`Tools/Unreal/Weather/AuthorVillageLandscapeBridge.py` 分别执行material/map，`VerifyVillageLandscapeBridge.py` 检查母材质48节点、World配置和硬引用路径，退出码0；报告 `Game/Saved/Validation/VillageSnowBridgeAssetAudit_20261011.json`。
- 最新客户端正式UAT `BuildCookRun`（`-skipbuild`，Client编译前置已单独返回0）Cook+Stage+Pak/IoStore退出0，客户端1054 Cook包。对应工件：`D:/SnowValidation20261011/VillageBridgeFinalClient20261011/Stage/WindowsClient`。
- 本轮服务器正式UAT `BuildCookRun`（`-skipbuild`）Cook+Stage+Pak/IoStore退出0，服务器556 Cook包。对应工件：`D:/SnowValidation20261011/VillageBridgeServer20261011/Stage/WindowsServer`。
- 针对最终客户端和本轮服务器用真实UnrealPak各自导出CSV，而非按源文件推测打包。执行 `Tests/Assets/ValidateVillageSnowCookedContainer.py` 通过：最新Client要求10/10资源、Server要求6/6资源，Server非法Surface/VFX/DBAPresentation/SnowReview资源0个。实际清单条目Client=1120，Server=557；机器可读报告 `D:/SnowValidation20261011/VillageBridgeFinalClient20261011/VillageSnowFinalCrossTargetCookReport.json`。
- Python质量门禁 `Tests/Architecture/ValidateVillageSnowBridgeIntegration.py`、素材Manifest SHA256复核、Python作者工具py_compile通过。新增原生测试源码 `DivineBeastsVillageSnowBridgeTests.cpp` 编译通过；该测试本轮尚未在UE Automation Runner单独执行，不冒称其引擎自动化通过。
- 用户还有其他编辑器窗口及大量未提交项目资源。本次未停止别的Editor、未更改其UI/角色/落叶素材、未重置工程、未向远程推送。

## 5. 未完成事项、验收操作和残余风险

1. **真正大雪画面**：在独立 `/Game/Development/Snow/Maps/L_DBA_SnowReview` 中用已配置HeavySnow/0秒过渡的审核Actor运行PIE，确认雪花可见、地形积雪不为纯白平板，并测试LightSnow、Clear及湿润过渡。SnowReview已有技术预览灰面和黑色离屏帧不构成通过证据，且先前运行曾被正式Login UI遮挡。
2. **正式Village在雪天的实际视觉**：本轮已验证正式地图的默认天气下MID五参数可写入和读回，尚需服务器实际产生HeavySnow快照后确认 `VillageSnowAmount` 确实升高、Shader显示正确，测试近远景Niagara及雪粉地形穿透。
3. **双客户端与专用服务器**：同一权威天气下的状态同步、晚加入、天气切换中途加入、断线重连、天气结束后回收效果；当前没有联机端到端证据。
4. **性能和人工签收**：分别记录真实高/中/低画质GPU时间、透明Overdraw、Shader开销及FPS，由用户在可见的PIE或正式客户端签名通过。本系统不具备代替真人审美验收的依据。
5. **风向细节**：目前风吹雪粉发射率依据SnowIntensity驱动，移动方向仍为固定表现向量而非实时天气风向矢量；本轮未增加第二套风场。

人工验收记录：`Docs/Implementation/SnowSurfaceManualReview_20261011.md`。项目插件接入规范：`Game/Plugins/DivineBeasts/DBAClient/Docs/VillageSnowBridgeIntegration.md`。当前状态：核心工程链路已通过，以上体验、联机和GPU门禁待进一步执行，不应标记全部完成。

## 6. 已执行的两个客户端联机与基本重连验证（补充）

- 已使用当前WindowsServer和WindowsClient正式Stage产物，在本机隔离端口19779启动1台专用服务器+2个独立客户端（NullRHI只影响显示，不跳过网络连接）。真实服务器日志分别记录两个 `Join succeeded`（加入成功）；两个客户端各自收到 `Welcomed by server`（服务器欢迎）及正式Village地图，并分别取得 `VILLAGE_SNOW_MID_VERIFY_SUCCESS`。证据位于 `D:/SnowValidation20261011/VillageNetworkSmoke20261011/` 的一份服务器和两份客户端日志。
- 同样以隔离端口19780运行1服+2端重连测试。最初两客户端均加入成功、MID回读成功；主动断开B客户端后，保持A客户端仍在线，重新启动B客户端，服务器累计出现**三次** `Join succeeded`，第二个B进程重新进入Village并再次取得 `VILLAGE_SNOW_MID_VERIFY_SUCCESS`。测试输出 `VILLAGE_RECONNECT_JOIN_AND_MID_PASS`；日志 `D:/SnowValidation20261011/VillageReconnectSmoke20261011/`。
- 此处通过的是**实际游戏专用服务器连接、地图载入、基本断线重进和每次客户端各自的MID雪资源装配**。未触发一组已确认HeavySnow的权威过渡，因此**不能**冒充降雪期间服务器快照同步、晚加入雪粒子生命周期、风向、正式账号恢复或外网弱网重连已经验收。
- 本次人工视觉预览尝试启动隔离UE Editor，但MonoLith插件与现有编辑器争用9316监听及共享索引数据库。已主动停止本轮新开的编辑器进程，没有停止用户其他编辑器。未获得可签收的无遮挡大雪画面、GPU性能数字。


