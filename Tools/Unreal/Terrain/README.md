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
8. LayerInfo：**十份已全部真实创建并由独立UE5.8进程反射验证**，资产位于/DBAWorldPack_Village/Terrain/LayerInfos/LI_DBA_Terrain_*，对应LS_*，均为FinalWeightBlending。原生GamePlatformSurfaceTerrainLayers命令已修复逗号CSV解析截断；Tools/Unreal/Terrain/VerifyLandscapeLayerInfos.py为只读独立验证工具。此次使用隔离后缀Editor DLL，未覆盖在用的原插件DLL；正常生产模块仍需待并发编辑器释放后正式重编。执行日志见Docs/Implementation/Terrain10LayerInfosCreation_20261011.md。
9. 审查地图：GamePlatformSurfaceTerrainReviewMap原生Commandlet在可用Editor模块下读取Raw/Height_1009_LE.r16与10×R8，使用ALandscape::Import创建真正1009平方、64组件、10层Landscape的独立/Game/Development/Terrain审查地图。不得覆盖正式L_Village_Start。地图保存、重开、DataValidation和人工截图必须额外验收。

10. GamePlatformPCGEditor的GamePlatformPCGTerrainBiomeGraphs通用Commandlet已形成源码，按LS_权重层阈值创建四张Graph；仍待UHT编译及真实PCG资产验证。
11. RunTerrainGaeaFullReview.ps1：默认DryRun，-Apply必须没有其他同项目Editor或UBT进程。其DryRun通过、并发状态下-Apply安全拒绝的行为已验证。脚本用于后续Editor构建、LayerInfo、过滤Graph、独立地图和UE回读，当前不能算作地图已创建。
12. 最终仍需Cook/Stage/UnrealPak、PCG真实生成与人工签收。
13. CheckGaeaTileSeams.py：对Gaea 2.3真实多瓦片R16与九张Mask做基于SHA256和明确Y方向的左右、上下接缝比对；--self-test只验证算法拦截故意制造的裂缝，当前未取得真实Gaea瓦片的可验收记录。

## 2026-10-11 新款CliffRock01（青灰悬崖岩）候选PBR制作

- AcquireCliffRock01Source.ps1：Poly Haven真实CC0裸岩Marble Cliff 05四张4096PNG，官方MD5与本地SHA256核对；原始源不能覆盖。
- GenerateCliffRock01BlueGrayAlbedo.py：只派生青灰BaseColor（线性亮度保留），NormalDX/ORM/16位Height原样保留；--verify校验可重复性。
- ReviewCliffRock01PBR.py：四张真实源图、2×2平铺、旧暖色与青灰对照和量化诊断，不假冒UE截图。
- AuthorCliffRock01UE.py：创建4张原生Texture2D、10节点PBR材质、预览MI和十层Rock专属MI；旧林地Rock01与Village地图均不覆盖。
- AuthorCliffRock01WorldAlignedUE.py：创建UE原生三向世界对齐14节点Material及20m扫描尺度MI；临时残缺母材质经过备份、单路径节点重建后恢复，未触碰其它资产。
- VerifyCliffRock01UEAssets.py：独立UE5.8进程验证4纹理、2 Material、3MI、函数数目、2000cm投射尺度及纹理引用。
- 限制：真实UE三向预览出现暗黑/棋盘格，仍需可控光照的实际悬崖审核关卡复测及GPU验收；人工签收待用户本人批准。说明见Docs/Implementation/TerrainCliffRock01PBRManualReview_20261011.md。

## 已经实际通过的证据（2026-10-11）

- 技术源PNG 50/50，纹理QA、SHA256、16位高程、单像素图层权重总和255全部通过。
- 49/49 UE真实Texture2D导入及回读通过，材质M_DBA_Terrain_MultiLayer10（146个表达式）与项目MI生成/Shader编译/UE回读通过。
- Gaea兼容Calibration输入转换：10张Weight PNG，1024个合法PCG Schema v1候选点、10张R8权重及16位R16，确定性与非法输入回归通过。实际Gaea文件尚不存在。
- 四份项目级PCG Blueprint资源与一份两节点四边的真实官方PCG采样Graph由UE Editor生成、独立进程重载验证通过；未证明真正的地形层权重可运行、更未验证网格实例化。
- GamePlatformSurfaceTerrainLayers与GamePlatformSurfaceTerrainReviewMap两份Commandlet均通过真实MSVC C++目标文件编译，退出码0；但Editor DLL被另一会话编辑器锁住，正式链接与执行仍未完成。禁止主动结束别人的UE编辑器。
- 十份真实Landscape LayerInfo均已保存及反射回读；完整UE三目标常规构建、独立地形测试地图/PCG实际生成、Client/Server新Cook及用户人工签收仍不能标记完成。

## 回退与安全
### 2026-10-12 PCG地貌语义V2（道路与排除区来源分离）

原`TerrainMaskRules.py（地形掩码投影）`的V1校准逻辑把`Exclusion（通用排除）`直接赋给`RoadDirt（道路土层）`，并将所有高排除区域的`Pcg.Exclude.Source`标记成`Road.Network（道路网络）`。这在测试时制造虚假的道路区域，也会让人工锁、禁建区误被当成道路。**不能把这种历史校准映射用于正式Gaea或Village生产。**

保留V1作为已经生成的技术校准数据（`CalibrationDerived`）的确定性兼容合同；新V2显式采用`projectionVersion=2`，`Gameplay.Exclusion（通用玩法排除）`独立于道路。V2仅在用户提供带SHA-256的独立`roadOverlay（道路覆盖源）`时生成`RoadDirt`。否则十层中的`RoadDirt`权重为0，道路由项目已批准Spline（样条）与WorldDirector（世界编排器）后续烘焙；不凭空绘路。正式Gaea 2.3来源投影默认V2，仍严格验证高度及9张原始掩码；技术校准必须显式`--calibration`且仍不能冒充真实Gaea。

已独立生成`DBAWorldPack_Village/SourceArt/Terrain/CalibrationDerivedV2（新手村第二版技术校准源）`下10张真正PNG、1,024条PCG候选点JSON、1份带源SHA-256及`projectionVersion=2`的投影清单，**不覆写V1导出或UE已保存的地形权重/材质/地图**。

```powershell
# 保留旧版技术校准资源完整性
python Tests/Architecture/ValidateTerrainMaskPCGProjection.py
# 验证V2道路/排除语义、新版源图、PCG Schema和来源完整性
python Tests/Architecture/ValidateTerrainMaskPCGProjectionV2.py
# 未来正式Gaea导出，需要实际2.3 Manifest和全套源SHA，指定未占用的全新输出目录：
python Tools/Unreal/Terrain/ValidateAndProjectGaeaExports.py --input-manifest <真实Gaea清单路径> --projection-version 2 --output-dir <全新源输出目录>
```

V2技术数据不经UE Editor导入、真实Landscape/PCG重开及双端Cook之前，不能认为正式Village世界地形/道路/林木已迁移。已有V1校准审查图不因新版本而自动被更新。


保留现有GamePlatformPCG Gold模板、原PCG蓝图、GamePlatformSurface原素材、正式Village雪MID及地图。该任务只创建独立的技术测试内容和通用Editor作者入口，禁用项目客户端外部目录写入，不删除、覆盖或推送其他人的资源。正式Gaea源导出、Cook及人工验收未通过不得上线。
