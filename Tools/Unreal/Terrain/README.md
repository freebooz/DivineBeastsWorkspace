# Gaea × Landscape × PCG 技术生产工具

所属：DivineBeastsWorkspace/Tools/Unreal/Terrain。唯一工程：Game/DivineBeastsArena.uproject（UE5.8），项目实际内容归DBAWorldPack_Village；平台底层机制使用GamePlatformSurfaceEditor与GamePlatformPCGEditor。

## 执行次序与边界

1. GenerateTerrainCalibrationPBR.py（生成10×3种技术PBR源纹理、16位高程、9个地貌Mask及10图层Weight）。所有生成图片及SHA256清单位于WorldPack/SourceArt/Terrain，仅属技术校准，绝非真实Gaea 2.3导出或最终3A扫描纹理。
2. Tests/Assets/ValidateTerrainCalibrationSource.py（源纹理边缘、尺寸、通道、权重检查），Generator --verify（只读确定性复验）。
3. ImportTerrainSourceTextures.py：环境变量TERRAIN_TEXTURE_IMPORT_MODE=apply，真实UE5.8 Editor导入49张Texture2D到WorldPack/Terrain，Height仍为真实Landscape输入，不伪装成Texture2D。ValidateOnly使用MODE=verify。
4. AuthorTerrainMultiLayerMaterial.py：TERRAIN_MATERIAL_AUTHOR_MODE=apply，生成146节点多层PBR母材质M_DBA_Terrain_MultiLayer10及MI_DBA_Terrain_Calibration，真实Shader编译、保存回读。此母材质使用30张1024高质量默认贴图：仅供独立技术审核地图，**不得直接迁入正式Village共享地图**，否则服务器可能被硬引用拖入高质资源。正式生产须先生成具有小型默认贴图的Server-safe地形母材质，客户端按SoftPath/租约覆写。
5. TerrainMaskRules.py、ValidateAndProjectGaeaExports.py：默认强制实际Gaea 2.3来源和坐标/源哈希；--calibration允许本项目技术样本，--verify-only无副作用，输出10张归一化Weight PNG及PCG Schema v1点样本，并不假装UE生成。输出位于WorldPack/SourceArt/Terrain/CalibrationDerived。
6. ExportTerrainLandscapeRaw.py：输出1009平方little-endian R16及10张R8权重原始数据和manifest，供原生UE Landscape::Import读取。--verify只读检查。
7. 真实Blueprint和PCG：AuthorTerrainPCGReviewBlueprints.py（TERRAIN_PCG_BLUEPRINT_MODE=apply）复用现有GamePlatformPCG放置器，生成四份项目级真实蓝图；AuthorTerrainPCGLandscapeSampler.py（TERRAIN_PCG_GRAPH_MODE=apply）生成官方Get Landscape Data→SurfaceSampler真实4边PCG图，并显式读取地形层权重。独立重开验证VerifyTerrainPCGReviewAssets.py。未执行网格Spawner/Production Bake，不能作为完整PCG已实现报告。
8. LayerInfo：原生Commandlet为GamePlatformSurfaceTerrainLayers，需先独立构建并成功装载GamePlatformSurfaceEditor模块，然后通过-LayerRoot=/DBAWorldPack_Village/Terrain/LayerInfos -LayerIds=Grass,ForestSoil,Dirt,Rock,Scree,Sand,Gravel,RoadDirt,FarmSoil,ShoreMud -AssetPrefix=LI_DBA_Terrain_ -LayerNamePrefix=LS_生成真实十份ULandscapeLayerInfoObject。-ValidateOnly只读验证；LayerName为UE只读反射字段，不得通过普通UE Python冒充创建成功。
9. 审查地图：GamePlatformSurfaceTerrainReviewMap原生Commandlet在可用Editor模块下读取Raw/Height_1009_LE.r16与10×R8，使用ALandscape::Import创建真正1009平方、64组件、10层Landscape的独立/ Game /Development/Terrain审查地图。不得覆盖正式L_Village_Start。地图保存、重开、DataValidation和人工截图必须额外验收。

## 已经实际通过的证据（2026-10-11）

- 技术源PNG 50/50，纹理QA、SHA256、16位高程、单像素图层权重总和255全部通过。
- 49/49 UE真实Texture2D导入及回读通过，材质M_DBA_Terrain_MultiLayer10（146个表达式）与项目MI生成/Shader编译/UE回读通过。
- Gaea兼容Calibration输入转换：10张Weight PNG，1024个合法PCG Schema v1候选点、10张R8权重及16位R16，确定性与非法输入回归通过。实际Gaea文件尚不存在。
- 四份项目级PCG Blueprint资源与一份两节点四边的真实官方PCG采样Graph由UE Editor生成、独立进程重载验证通过；未证明真正的地形层权重可运行、更未验证网格实例化。
- GamePlatformSurfaceTerrainLayersCommandlet首次编译其C++源代码成功，但编辑器DLL被另一会话运行中编辑器锁定，链接LNK1104；新地图Commandlet源已创建，但尚未编译/执行。禁止主动结束别人的UE编辑器。
- 完整UE三目标构建、真实Landscape LayerInfo、地形测试地图/PCG生成、Client/Server新Cook及用户人工签收仍不能标记完成。

## 回退与安全

保留现有GamePlatformPCG Gold模板、原PCG蓝图、GamePlatformSurface原素材、正式Village雪MID及地图。该任务只创建独立的技术测试内容和通用Editor作者入口，禁用项目客户端外部目录写入，不删除、覆盖或推送其他人的资源。正式Gaea源导出、Cook及人工验收未通过不得上线。
