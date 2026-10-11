# 神兽联盟 CliffRock01：青灰裸露绝壁PBR制作与视觉审核（2026-10-11）

项目：DivineBeastsWorkspace / UE5.8。归属：第三层 DBAWorldPack_Village（新手村内容包）。
**结论：同源4K PBR技术制作和UE真实资产保存、独立加载验证已完成；最终AAA真实悬崖画面、GPU性能和用户本人签收尚未通过，因此不得发布/替换正式Village。**

## 一、素材来源与美术定位

林地Rock01保留为深褐风化、含落叶的林地岩面备用，不修改。CliffRock01专为湖心三岛环形绝壁、瀑布两侧山壁、山体裸岩，要求青灰色、无落叶泥块、自然断层及裂隙。
本款选用Poly Haven CC0真实扫描Marble Cliff 05（marble_cliff_05），来源 https://polyhaven.com/a/marble_cliff_05 ，扫描宽约20米。四张同源4096×4096原图：BaseColor 100056028字节、NormalDX 95724128字节、ORM 43406011字节、16位Height 18353643字节，总计257539810字节；官方MD5与本地SHA256均通过。
项目另行生成只修改BaseColor的青灰版（不改原扫描），sRGB→线性光空间→低饱和青灰转换→sRGB，平均RGB由(147.3,140.2,130.8)变为(124.3,132.4,141.1)，派生SHA256为813221888826b714c1b9faece49d4d133868d93726d57699d5bff3294a272af6。原始图片及证据保存在项目的SourceArt/Terrain/PBR/CliffRock01目录，原图不可覆盖。来源清单与颜色派生清单均明确productionAssetApproved=false（待审核）。

## 二、已经创建的UE5.8原生资产

UE挂载目录：/DBAWorldPack_Village/Terrain/Realism/CliffRock01
- Textures/T_DBA_CliffRock01_BaseColor_BlueGray：真4K Texture2D、sRGB开、TC_DEFAULT。
- Textures/T_DBA_CliffRock01_NormalDX：真4K Texture2D、sRGB关、TC_NORMALMAP，保留原始DirectX法线。
- Textures/T_DBA_CliffRock01_ORM：真4K Texture2D、sRGB关、TC_MASKS，R=AO、G=Roughness、B=Metallic。
- Textures/T_DBA_CliffRock01_Height：真4K Texture2D、sRGB关、TC_GRAYSCALE，保留16位源高度但未启用Nanite位移。
- Materials/M_DBA_CliffRock01_PBRReview：10个UE PBR节点，基础色/法线/环境遮蔽/粗糙度/金属度均连接。
- Materials/MI_DBA_CliffRock01_PBRReview：预览材质实例。
- Materials/MI_DBA_Terrain_CliffRock01：继承既有M_DBA_Terrain_MultiLayer10，**仅Rock层替换PBR参数**，其他九层不变。
- Materials/M_DBA_CliffRock01_WorldAligned：14节点世界对齐三向投射UMaterial，调用官方WorldAlignedTexture/WorldAlignedNormal三次函数，世界空间法线、ORM物理分通道。
- Materials/MI_DBA_CliffRock01_WorldAligned：20米（2000cm）真实扫描尺度的项目三向投射MI。

三向投射原生制作中曾因UE5.8 ComponentMask反射输入名不匹配而创建不完整资产。已备份独占候选.uasset，恢复时只重建本次资产节点，不改旧Rock01、正式Village、平台插件；重新完成UE Shader编译、保存。**独立新UE5.8进程**运行VerifyCliffRock01UEAssets.py，实际回读确认四张4096图的压缩和sRGB、两个UMaterial（10/14节点）、三个MI与9个纹理引用、三次世界对齐函数和2000cm世界尺度；日志Game/Saved/Logs/CliffRock01IndependentReload20261011.log显示CLIFFROCK_UE_COMPLETE_CANDIDATE_PASS，进程退出码0。

## 三、人工视觉审核与真实证据

审查目录：Game/Saved/Validation/TerrainGaea/CliffRock01
- 01_CliffRock01_PBR4Maps_Display.jpg：青灰BaseColor、真实DirectX法线、原始ORM、保真16位Height的正确显示预览。
- 02_CliffRock01_BaseColor2x2_Display.jpg：原图2×2重复平铺；相邻裂隙在四块之间明显重复，需要远景去重复策略。
- 03_CliffRock01_OriginalVsBlueGray_Display.jpg：真实原色/派生蓝灰色/简化法线方向光对照（第三项非UE渲染）。
- CliffRock01_SourceVisualMetrics.json：抽样法线色6193种、粗糙度标准差0.0307、基础色接缝RMSE与相邻像素变化比约1.316；真实源色图无叶片，纹路更适合裸露绝壁。
- CliffRock01_UE5_8_CliffVsForest_Sphere.png、CliffRock01_UE5_8_RealMaterial_Plane.png：真实UE材质预览；HDRI亮度偏暗。
- CliffRock01_UE5_8_WorldAlignedVsUV_Cube.png、CliffRock01_UE5_8_WorldAligned20m_Plane.png：真实UE预览已生成，但出现异常暗黑/类似默认棋盘格，**不能判定三向世界投射视觉成功**。真实图中未可充分判断材质拉伸和宏观重复，后续必须在可控方向光环境/20米悬崖测试几何上排查并复检。

视觉初步意见：配色青灰、裂隙完整、无落叶、法线及高度形态优于旧技术校准纹理，可保留为CliffRock01第一版候选。源图采样、资产身份与Shader保存回读达技术标准；世界对齐材质在实际场景的可视品质未通过。严禁把Shader成功误当作AAA美术通过。

## 四、仍待真正完成的门禁

1. 在明亮、可控光照下重新验证三向投射；实际能看见青灰裸岩、无黑色棋盘格和明显垂直拉伸。
2. 在独立CliffRockReview地图真实20m悬崖上检查法线、Roughness、远近变化、雪/湿润/苔藓及PC高/中/低质量的GPU开销。
3. 正式Village世界服务器安全Cook及客户端高清软装配完成之前，禁止将大容量纹理MI硬引用到正式共享地图。
4. 用户真人确认色调、断层尺度、平铺重复和场景效果；SourceManifest和ColorGradeManifest中的productionAssetApproved继续为false。
5. 本轮仅创建独立候选资产与验证脚本，不删除旧Rock01、不覆盖正式L_Village_Start、不擅自提交或推送。

接续审核文档：Docs/Implementation/TerrainPBRMaterialsReviewLedger_20261011.md。下一张材质Grass等须按单张人工签收要求排队，不能把一张未签的Cliff源当作十类都完成。
