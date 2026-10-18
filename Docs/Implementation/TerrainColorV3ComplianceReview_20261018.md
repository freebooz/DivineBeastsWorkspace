# SSLM 十层地形纹理与材质 V3.0 合规审查及实际整改记录
## 2026-10-18 后续真实UE资产收口：十层V3正式基础色已完成10/10

此节为当前最新事实，以下9/10段落仅保留历史阶段证据。此前Dirt（普通泥土）一项因其它UE编辑器文件锁阻塞；用户要求继续后，已在文件独占条件下修正旧校验，允许来自原生验证的81节点Texture Bombing（纹理轰炸）共享材质新SHA，不放宽正式地图SHA、旧回退备份及Texture2D来源校验。随后使用正式UE5.8 Editor第一次真实重导入Dirt，第二次独立进程完成十层V3正式纹理全量冷重载，工具输出 SSLM_V3_FORMAL_10_COLD_VERIFIED（十层冷重载通过）。不再存在待导入的Dirt层。

完成证据：Saved/ArtReview/TerrainColorV3FormalCover/AppliedManifest.json（正式十层清单，rows=10、coldVerifyPassed=true），Game/Saved/Validation/TerrainColorV3FormalCover/FormalDirt_UEApply_AfterBombingBridge.log（泥土原位Texture2D重导入日志）、Formal10_UEColdVerify_AfterBombingBridge.log（另进程完整十层真实回读日志）。首次过期安全门禁的失败日志FormalDirt_UEApply_Final.log保留，以便审计为何修复SHA合同。原十张基础色和正式地图及旧母材质的12份执行前备份、原40张PBR扫描来源仍可追溯；不会重新生成法线、ORM及Height（高度）资源。

特别说明：正式Village共享母材质已另经用户授权升级为81表达式三通道Texture Bombing，且本轮在保持正式L_Village_Start地图SHA不变的条件下，导入并绑定新的256×256 V3自然绿色共享默认Texture2D；这是有意的母材质原位更新，因此下方旧阶段“材质文件不变”只对当时执行范围成立。完整二次原生材质回读和共享世界引用隔离结果，见VillageTerrainTextureBombingV3_20261018.md（新手村纹理轰炸实施记录）。

尚未通过GPU视觉/发布验收：原六态D3D12截图像素黑帧；新独立D3D12三源、启动地图预加载三源及常规GUI视口跨帧截图仍无有效地形RGB像素，不能宣称最终场景色彩通过。Server Cook因其它Foundation构建/烘焙持有同一引擎互斥锁，本轮状态NotExecuted（未执行），原生UE地图依赖审计与专服剥离策略静态检查通过，但不能替代Client/Server容器审计。人工视觉签收和FPS/GPU耗时验证仍须真实可见客户端画面才能进行。

## 2026-10-18 历史阶段快照（当时9/10，已由上方真实收口记录取代）

用户明确要求直接覆盖地形配色。**当前状态：十层目标9层已在UE5.8原位重新导入并通过另一个独立编辑器进程冷重载；Dirt（泥土）1层仍未更新，原因是其他进程占用文件（Windows错误码32）。不可将9/10视为全部完成。**

已覆盖并通过独立原生回读：Grass（草地）、ForestSoil（林地土）、Rock（岩石）、Scree（碎石坡）、Sand（砂地）、Gravel（砾石）、RoadDirt（土路）、FarmSoil（耕地）、ShoreMud（岸湿泥）。保持十种地形Texture2D稳定路径，只重导入V3派生BaseColor（基础色）；旧法线Normal、ORM（遮蔽/粗糙度/金属度）、Height（高度）原扫描PBR贴图不修改。正式Village地图L_Village_Start.umap和共享M_DBA_Village_LandscapeBridge.uasset执行前后SHA256未改变；不改地图碰撞、导航、地形权重、PCG和权威服务器逻辑。

**正式Village实际客户端装配**：复用原DivineBeastsVillageSnowWorldSubsystem（新手村客户端雪/地表桥接），将草地基础色软引用改成DBAPresentationPack_Core/Materials/Terrain/TexturesV3中的V3草地纹理，同时关联该CC0扫描的WorldPack Normal及ORM；VillageDevelopmentClient配置中写入软路径与客户端Cook目录。不会将客户端高精函数化母材质硬绑定服务器共用地图。修改的是项目客户端源码和配置，尚未运行新客户端Build/Cook与真实客户端视觉审查，不得声称玩家已看到新地面。

**资源隔离**：已将DBAPresentationPack_Core/Materials/Terrain及旧草地客户端目录加入Build/Game/ServerPresentationExclusions.json（服务器表现目录排除规则真源），更新DedicatedServer、VillageServer、VillageDevelopmentServer三个受管配置和专服Pak规则；统一生成后的策略检查通过，未执行真实Client/Server Cook与UnrealPak容器审计。

**恢复、回退与实际证据**：
- 10项原纹理、Village正式地图、共享母材质共12份执行前二进制与SHA清单，保存在Saved/ArtReview/TerrainColorV3FormalCover/Before_20261018/BackupManifest.json。
- 原生执行工具：Tools/Unreal/Terrain/ApplyTerrainV3ToFormalTexturesUE58.py；使用真实UE ImportAssetTask而非伪造uasset。实际九项进度为Saved/ArtReview/TerrainColorV3FormalCover/ProgressManifest.json，partialColdVerifyPassed=true、coldVerifyPassed=false、humanReviewPassed=false、pending=Dirt。
- 原生日志：Game/Saved/Validation/TerrainColorV3FormalCover/Formal10_UEApply_20261018.log（首轮在Dirt保存遇文件锁）、Formal7_UEApply_20261018.log（七项独占覆盖）、Formal9_UEColdVerify_20261018.log（九项独立冷重载）。九项存盘、sRGB、尺寸、来源软路径和磁盘SHA均通过原生回读。
- 通过静态检查：TestTerrainV3FormalCover.py、ValidateVillageSnowBridgeIntegration.py、TestTerrainColorV3Policy.py、ValidateTerrainProductionIntegration.py，以及ServerPresentationPolicy.py check。

**剩余任务**：文件锁释放后，仅针对Dirt执行独占UE5.8原位导入，再使用另一UE进程检查全部十层。旧六态D3D12场景截图全部为黑帧，GPU/Shader、RVT/PCG视觉融合、移动端与多画质、Client/Server Cook、人工前后对照审查尚未通过。本次用户授权直接覆盖不等于自行放行正式发布质量门禁。下文“不得覆盖正式地图”保留为先前候选制作阶段历史约束，实际本次亦未改动地图文件。
**剩余Dirt安全恢复入口**：Tools/Unreal/Terrain/ResumeTerrainV3FormalCover.ps1（PowerShell脚本）。默认只读验证12份原资产、九层冷重载报告与Dirt文件锁；仅显式-Apply且Dirt不再被占用、同工程UE编辑器已安全关闭时，才由UE5.8真实导入Dirt，并由第二个独立进程完整冷重载全部十层。检查真实日志成功标记及Python异常、十层SHA/清单；单凭UE退出0不得宣告通过。当前只读预检结果file_locked=True，未执行-Apply。



日期：2026-10-18；工作空间：DivineBeastsWorkspace；虚幻引擎 UE5.8；当前分支 main。所属：地形/材质/环境色彩专项，不覆盖正式地图。

**总论：原有十种地表扫描基础色与[全局V3.0配色规范](../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)存在明显差异。已经真实派生十层候选纹理、新建十张UE纹理、1份母材质、4份MI以及独立审核Landscape；独立UE冷重载通过，但六态D3D12截图均为黑帧，最终视觉质量、性能、人工美术验收暂未通过。**

## 一、十层原始色值与V3候选对照

源数据：40份Poly Haven CC0扫描原图来自Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json；2048×2048基础色均值是源PNG的规则采样结果，**不是UE真实光照后的画面颜色**。V3色值仅为初始屏幕sRGB参考，不能作为线性BaseColor直接使用。

| 图层 | 原始基础色均值 | V3初始屏幕参考 | 候选基础色均值 | 主要发现 |
| --- | --- | --- | --- | --- |
| Grass 草地 | #978459 | #6B9956 | #6F9857 | 原始黄褐，候选纠正为自然青绿 |
| ForestSoil 林地土 | #58442E | #A88F6D | #816A4E | 原始暗棕，候选增亮但仍保留林地深色 |
| Dirt 泥土 | #79643C | #A88F6D | #9A8260 | 偏暗黄，候选暖褐 |
| Rock 岩石 | #787164 | #89988C | #81897E | 偏暖灰，候选转为灰青 |
| Scree 碎石坡 | #4E3F1F | #A8A38C | #8F8976 | 暗褐且噪声强，二次候选恢复自然灰感 |
| Sand 湖岸砂 | #6C604D | #C5AD83 | #AD9874 | 偏灰暗，候选增加明度 |
| Gravel 砾石 | #928368 | #B9B6A0 | #AAA48E | 偏暗，候选自然米灰 |
| RoadDirt 土路 | #473423 | #B7A181 | #A69276 | 道路暗黑，候选改善通行可见性 |
| FarmSoil 耕地 | #5B4B31 | #A88F6D | #867153 | 深色保留部分，不做不合理同色化 |
| ShoreMud 湿泥 | #53493B | #79705C | #685F4D | 适度增亮，保留湿土明度层次 |

候选色值不是强制追求全部像素一致：湿泥/耕地可偏暗，石材/草地应保留真实PBR的局部细节。视觉合规仍需同一固定曝光、真正三维游戏镜头和人工审核，不能单靠纹理均值判断。

## 二、实际新建的源图、母材质与索引

- Tools/Unreal/Terrain/TerrainV3ColorMapping.json（以权威色板语义ID驱动十层校正参数，禁止复制竞争色板）。
- Tools/Unreal/Terrain/GenerateTerrainV3ColorSource.ps1（线性颜色曲线、色相一致性/微对比度调整，仅创建候选，不覆盖扫描原图），十张实际2048² PNG输出：Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Terrain/V3/。
- Saved/ArtReview/TerrainColorV3/GenerationManifest.json（原图/派生/规范SHA-256来源）；同目录 TerrainV3_BeforeAfterReview.png（三栏离线人工审稿板）及 FirstPass/（保留异常偏色碎石初版，供追溯）。
- Tools/Unreal/Terrain/AuthorTerrainV3ColorUE58.py（UE5.8真实资产创建和独立加载校验）；新增十个Texture2D挂载至 /DBAPresentationPack_Core/Materials/Terrain/TexturesV3/。
- 真实UMaterial：/DBAPresentationPack_Core/Materials/Terrain/M_DBA_Terrain_Production10_V3。
- 真实MI四份：/DBAPresentationPack_Core/Materials/Terrain/MI_DBA_Terrain_Production10_V3_Review、_High、_Medium、_Low；继续继承原函数化高精母材质的有效质量配置，不等于GPU性能改善。
- 实际UE审核图：/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review；三个独立UE进程完成map_copy、map_bind、map_verify，唯一Landscape使用新的V3_Review MI。原L_DBA_TerrainFunctionReview.umap前后SHA相同。
- Tests/Assets/TestTerrainColorV3Policy.py（颜色语义/十层/原始40图SHA/实物15资产文件/正式地图隔离门禁）；Tests/Assets/TestTerrainColorV3GPU.ps1（黑帧、重复帧阻断门禁）；Tools/Unreal/Terrain/CaptureTerrainColorV3GPUReview.py（复用既有RHI采集）。

**物理与架构保护**：只把原函数化母材质的十个Terrain*BaseColor（地表基础色纹理）参数重定向至新sRGB基础色；Normal（法线）、ORM（遮蔽/粗糙度/金属度）、Height（高度）、LS_*权重、雪/苔藓/湿润PBR和8处平台MaterialFunctionCall维持旧合同。GamePlatformSurface不反向依赖生肖/项目内容；新高精资源仅属于第三层客户端DBAPresentationPack_Core，原10层扫描与新手村权威碰撞仍在已有职责内。

## 三、真实测试与不合规项

**已经通过**：十张候选源PNG生成及-VerifyOnly SHA复核；原始10×4扫描纹理SHA完整；Tests/Assets/TestTerrainColorV3Policy.py --ue-files返回合规静态门禁通过和反例自测通过；真实UE创建保存10 Texture2D+1 UMaterial+4 MI；独立UE Editor冷重载及十层映射、平台8函数和原Normal/ORM/Height引用回读通过；独立新地图三进程保存及冷重载均退出0。

日志：Game/Saved/Validation/TerrainColorV3/UEColdVerify_Engine.log（含SSLM_TERRAIN_COLOR_V3_UE_DONE mode=verify）与 Review_map_{map_copy,map_bind,map_verify}_UE.log。

**未通过（严格阻断）**：在真实UE5.8 D3D12命令行场景中调用SceneCapture2D并写出六张1280×720 PNG，但逐像素统计每张中央区域平均亮度只有约**0.09/255**，黑像素比例**100%**；Moss/Wet/LightSnow/HeavySnow/AfterSnow五种状态SHA完全相同。结果保存在Game/Saved/Validation/TerrainColorV3_GPU_20261018/TerrainColorV3_GPUFrameQA.json，pass=false，issues=7（6个黑帧与1组重复帧）。黑帧不能成为有效颜色对照，不能因磁盘有PNG就宣称RHI视觉审核通过。

**既有缺口与风险**：旧PBR项目报告已登记Rock/Scree法线和RoadDirt基础色的2×2接缝风险，尚未逐一修复；地形RVT适配缺少生产回读证据；PCG真正Spawner/Bake、Nanite岩石和Landscape材质过渡、正式Gaea 2.3数据、独立Client/Server Cook资产剥离、打包客户端/移动端GPU实测和人眼签收均未完成。十层地形色彩修改不能替代这些专项验收。

## 四、人工审核操作入口与生产准入

1. 在本地打开Saved/ArtReview/TerrainColorV3/TerrainV3_BeforeAfterReview.png，逐层比较源扫描图、V3派生图、初始sRGB参考色；发现岩坡色斑、草地过绿、道路太亮等问题时登记整改，不直接覆盖旧扫描素材。
2. 在正确UE5.8工程的Content Browser（内容浏览器）中，打开 /Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review（V3审核地图），与 /Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview（原函数化地形审核地图）对照。同机位、固定曝光、相同主光与画质，检查草地/道路/岩层、近远距、PCG与岩石、水岸以及雪/苔/湿润。
3. 先修复D3D12离屏黑帧问题（需要核查Editor-Cmd渲染线程/SceneCapture的RHI有效性、曝光或场景摄像机），随后再做六态真实屏幕像素回归、Shader Complexity、GPU Profile及不同画质和移动端性能。
4. 必须取得用户人工审核意见，记录Before/Target/After、关卡路径、镜头、设备、Color Grade、材质PBR和性能证据，才能申请将候选视觉装配进正式新手村/主城/竞技场。
5. 正式Village的L_Village_Start仍使用中性共享Landscape母材质，**当前不得直接用纯客户端高精V3材质替换权威地图**，必须在Editor/Client/Server三目标Cook验证服务器排除客户端纹理后设计客户端表现层的安全装配。

## 五、回退、版本与当前状态

本轮不修改正式地图、不覆盖40份原扫描PBR PNG及旧纹理，也不删除已审核旧母材质或已有PCG/LayerInfo。V3候选位于全新独立路径，旧函数化审核地图及原有MI仍可正常使用。测试失败仅阻断生产迁移，不回退已通过真实UE冷重载的新候选资产。

**本次阶段结论：十层颜色整改候选=已实际创建；PBR源/材质合同与独立地图UE回读=通过；离屏截图六态视觉门禁=不通过；真人审美合规与GPU帧率、RVT/PCG衔接、Cook=待完成。**

参考：[2026-10-17十层函数化材质实施记录](TerrainMaterialFunctionIntegration_20261017.md)；[2026-10-16十层正式扫描PBR记录](TerrainProductionPBRExecution_20261016.md)；[当前V3全局色板主规范](../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)。
