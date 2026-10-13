# SSLM Rock01 写实岩石材质候选：真实制作与人工审核记录

日期：2026-10-11。UE5.8主工程：DivineBeastsArena。当前状态：技术资源创建及原生UE回读通过，**项目3A最终美术验收仍待用户确认**。正式新手村地图未改。

## 官方素材与许可

第一款Rock01采用Poly Haven提供的同源岩壁扫描 Rock Face 04（rock_face_04），来源 https://polyhaven.com/a/rock_face_04 ，CC0许可，作者 Dimitrios Savva，扫描面宽约2米。四张实际4096×4096 PNG：BaseColor基础色、NormalDX DirectX切线空间法线、ORM合并纹理（R遮蔽G粗糙B金属）、16位Height高度。官方MD5与本地SHA256已经全部通过；下载总量266,887,620字节。项目世界内容包SourceArt/Terrain/PBR/RockRealism01下有原图及完整来源Manifest，productionAssetApproved=false。

## 原生UE5.8资产

实际创建目录：/DBAWorldPack_Village/Terrain/Realism/Rock01

- Textures/T_DBA_RockRealism01_BaseColor：Texture2D，4096×4096，TC_DEFAULT，sRGB开启。
- Textures/T_DBA_RockRealism01_NormalDX：Texture2D，4096×4096，TC_NORMALMAP，sRGB关闭。
- Textures/T_DBA_RockRealism01_ORM：Texture2D，4096×4096，TC_MASKS，sRGB关闭。
- Textures/T_DBA_RockRealism01_Height：Texture2D，4096×4096，TC_GRAYSCALE，sRGB关闭。
- Materials/M_DBA_RockRealism01_PBRReview：预览用真实UMaterial，8个已连接节点，BaseColor/Normal/AO/Roughness/Metallic实际PBR通路；Shader编译与保存通过。
- Materials/MI_DBA_RockRealism01_PBRReview：完整扫描纹理预览MI。
- Materials/MI_DBA_Terrain_RockRealism01：继承现有十层主材质，独立覆盖Rock三张物理纹理的候选MI；不影响其他九种地表，也未将高精Texture2D硬引用写入正式Village共享地图。

Monolith原生UE脚本返回ROCK_REALISM_UE_MATERIALS_VERIFIED；独立再次反射四张4096尺寸、compression和sRGB以及Material/MI类与真实Texture参数；全部通过。

## 实际打开审核的资源

Game/Saved/Validation/TerrainGaea/RockRealism01目录：
- 01_RockRealism01_PBR4Maps_HeightCorrected_Display.jpg：四张原始PBR数据预览；高度16bit经显示归一，非贴图修改。
- 02_RockRealism01_BaseColor2x2_Display.jpg：2×2无缝/重复性审核。
- 03_RockRealism01_BeforeAfter.png：旧技术Rock与实物扫描新Rock色图及离线法线亮度对照，右侧不是UE实时截图。
- RockRealism01_SourceReviewMetrics_HeightCorrected.json：技术质量数据。
- RockRealism01_MaterialGrid_Sphere.png：真实Editor材质球预览，但当前HDRI球亮度较暗，不能单独用于判定最终真实地表色彩或GPU性能。

注意：前面生成的十材质绿色“审核通过”概念海报不是本次真实贴图或UE GPU签收证据，不用于实际工程导入。

## 本轮人工视觉意见

| 事项 | 结果 |
|---|---|
| 4K、来源可追溯及四图同源 | 技术通过 |
| UE Texture2D尺寸、法线方向、ORM物理通道、Shader与MI引用 | 技术通过 |
| 法线凹凸 | 明显优于原四颜色的平坦技术法线，抽样3857种不同法线色；通过源图细节审查 |
| 岩石裂隙与微纹理 | 自然细裂纹、剥蚀细节及裂隙可辨；通过源图审查 |
| 重复平铺 | 2×2无明显硬接缝；仍有大块岩面重复，需要正式景观宏观扰动 |
| ORM粗糙度 | 变化标准差约0.0178，较小；需真实室外灯光和干湿转换复审 |
| 适合区域 | 适合桃林/森林局部风化岩面，不能直接泛用湖心环形绝壁 |
| 风格风险 | 大面积深褐泥质和可见落叶，不能表达所有裸露悬崖的青灰硬岩 |
| UE真实Landscape及GPU帧时 | 未进行；不能声明已达3A最终质量 |
| 用户人工最终签收 | 待用户同意；productionAssetApproved继续为false |

## 下一步

请选择：一、接纳Rock01作为森林风化岩面备用资源，再制作更适合悬崖的青灰RockCliff；二、要求重新选择无落叶灰色岩壁作为Rock通用版；三、只调整本次候选的色调或平铺尺度后再审核。

用户未签收前不替换已有Rock校准Texture2D、十层主材质、不修改正式Village地图、不进入正式发行Cook。
