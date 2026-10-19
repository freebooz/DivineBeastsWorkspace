# SSLM十层地形正式PBR实施复核（2026-10-16）

版本：2026-10-16。工作空间：`DivineBeastsWorkspace`；虚幻引擎：UE5.8，实际根目录 `D:\UnrealEngine-5.8.0-release`。
本报告记录实际执行与验证证据，续接`TerrainProductionPBRIntegration_20261012.md`历史记录；**资源存在、脚本执行、Shader编译、GPU渲染、Cook和用户审美签收是彼此独立的状态**。

## 一、保护与回退

- 本轮未停止其他用户Editor/UBT，没有重置他人未提交代码、狼/UI资产，没有修改正式`/DBAWorldPack_Village/Maps/L_Village_Start`或它的`M_DBA_Village_LandscapeBridge`共享材质。
- 历史`Game/Saved/Validation/TerrainPBR_20261012/Rollback_TerrainCalibration_SourceAndUEAssets.zip`缺失；从Git历史`7d7d0b8f`读取30个技术原PNG且全部与旧Manifest SHA256相符，加30个当前干净版本UE纹理和1个旧Manifest形成61项新回退压缩包；ZIP CRC与历史清单哈希完成核对，总体约13.76 MB。
- 审计`Game/Saved/Validation/TerrainPBR_20261012/RollbackRescueAudit_20261016.json`含归档指纹。该压缩包位于Git忽略目录，**不等于异机灾备**，正式上线前应复制到受控备份存储。

## 二、实际制作

1. 40张Poly Haven CC0正式扫描源（10种地表×BaseColor/Normal/ORM/Height）通过独立Python图片内容与签名审计；三项未解决2×2平铺接缝：Rock法线、Scree法线、RoadDirt基础色。
2. 在真实UE5.8编辑器中导入40个Texture2D资产，保存到`DBAWorldPack_Village/Terrain/Textures`；新建10个Height原生资产。本轮头部编辑器进程输出`TERRAIN_PBR_UE_40_TEXTURES_VERIFIED mode=apply count=40`，后续发现源尺寸检验误用`blueprint_get_size_x`，其无有效平台Mip驻留时只有32。实际独立UE `AssetRegistry`的`Dimensions`标签返回`2048x2048`（抽检草地和岩石各4张）；已改为冷重载读原生源尺寸标签和导入源绝对路径，不再把平台Mip尺寸误当源纹理大小。
3. `GamePlatformSurfaceEditor`和`GamePlatformPCGEditor`限定模块MSVC编译和链接成功（本轮完整生产流水线最初编译8个动作），不等于完整Client/Server构建。
4. 修复正式PBR编辑器作者代码两处UE5.8真实反射入参：TextureSample的UVs/Coordinates兼容，StaticSwitchParameter的True/False输入。已经真实保存`/DBAPresentationPack_Core/Materials/Terrain/M_DBA_Terrain_Production10`与`MI_DBA_Terrain_Production10_Review`；独立新UE编辑器进程真实重载输出`TERRAIN_PRODUCTION_MATERIAL_UE_VERIFIED ... nodes 262`。母材质具40纹理输入、共用Wrap采样器、可选HeightBlend及平台MPC雪/苔藓/湿润桥。
5. 原生作者流水线`RunTerrainGaeaFullReview.ps1 -Apply -ProductionPBR -EngineRoot D:\UnrealEngine-5.8.0-release`成功，末尾输出`TERRAIN_GAEA_REVIEW_EDITOR_TARGETS_PASS productionPBR=True`；保存四份Forest/Rock/Shore/Road的`PCG_DBA_Terrain_*WeightFiltered.uasset`、独立1009×1009十层Landscape关卡`/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview`及4类PCG放置器绑定。**图绑定不等于网格Spawner生成/PCG Bake**。
6. 原`CalibrationDerivedV2`已补正旧源Manifest SHA与原有PCG JSON指纹；新的`CalibrationDerivedV2/Raw`完整输出1份R16高程、10份R8地表权重以及逐项SHA清单。项目独立V2测试通过：每个1009×1009像素权重和255；`Gameplay.Exclusion`与道路来源分离，无RoadOverlay时RoadDirt全部为0；1,024个PCG Schema样本为技术调试数据，**不是真实Gaea导出**。
7. 新增`Tools/Unreal/Terrain/AuthorTerrainProductionQualityProfiles.py`：项目层纯客户端High/Medium/Low候选MI的创建、静态高度混合开关及参数读回脚本。该脚本目前仅完成源码/Python语法及静态合同审核，**未在UE编辑器执行，不代表三档原生材质资产已生成或性能达标**。

## 三、验证与尚未完成门禁

通过：
- `Tests/Assets/ValidateTerrainProductionPBR.py`：40张正式PBR源图真实性与通道/动态范围、MD5和SHA审查，3项接缝列入人工QA。
- `Tests/Architecture/ValidateTerrainMaskPCGProjectionV2.py`：道路和排除语义分离通过。
- `Tools/Unreal/Terrain/ExportTerrainLandscapeRaw.py --projection-version 2 --verify`：1×R16、10×R8、逐点和255。
- `Tests/Architecture/ValidateTerrainProductionIntegration.py`：十层扫描、三层资源合同、回退、V2 Raw和正式地图硬引用边界检查通过；当前磁盘报告40 Texture2D、4权重图和1审核地图。
- 真实UE原生制作流水线与母材质独立冷重载成功。

阻塞或未完成：
- **完整冷重载**：`RunTerrainGaeaFullReview.ps1 -ValidateOnly -ProductionPBR`首次因用`blueprint_get_size_x`误检导入尺寸失败，已定位并修复；第二次运行时有其他会话正在使用本项目`UnrealEditor.exe`（PID当时56564，日志`Saved/Validation/LoadingBar20261016/HWebEditor.log`），按独占编辑器门禁拒绝并行；本报告不将完整冷重载写为通过。
- `AuthorTerrainProductionQualityProfiles.py`对应高/中/低MI的真实创建、独立保存和Shader验收仍待无遮挡编辑器窗口。
- 真实D3D12六态（晴、苔、湿、小雪、大雪、融雪）RHI捕获、像素对比、Shader Complexity与ProfileGPU以及三档GPU耗时未完成；不推断节省毫秒或达到60/120FPS。
- PCG Graph真实Generate（点）、StaticMeshSpawner、持久烘焙与实例剥离，真实Gaea 2.3高度/Mask对齐、RVT原生绑定、Nanite悬崖及景观人工艺术签收均未完成。
- 当前批次未执行完整Editor/Client/Server构建与干净Cook/Stage/IoStore/UnrealPak独立目标审计；高精贴图在共享WorldPack的Server打包隔离仍为待验证，不能推断只因材质实例在纯客户端包内就完成Server裁剪。

## 四、可复现命令

使用锁定UE5.8，在没有其他同工程Editor/UBT进程的安全窗口执行：

```powershell
# 只读：源码与层级合同
& 'D:\UnrealEngine-5.8.0-release\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' Tests/Architecture/ValidateTerrainProductionIntegration.py
# 独立UE反射回读已保存的正式资源，不做资产修改
& Tools/Unreal/Terrain/RunTerrainGaeaFullReview.ps1 -ValidateOnly -ProductionPBR -EngineRoot 'D:\UnrealEngine-5.8.0-release'
# 三档质量候选需经正式UE Python作者工具显式apply，再在新进程verify
# 六态GPU必须为真实D3D12 RHI；用CaptureTerrainProductionGPUReview.py和ValidateTerrainProductionGPUFrames.py独立审核
```

约束：当前仍不得把正式高精多层审查MI硬引用到专用服务器共享Village地图；不能将独立技术校准审核图标记为正式Gaea地图。
