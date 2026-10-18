# 《SSLM》Gaea × Landscape多层材质 × PCG一体化实施执行计划
日期：2026-10-11｜唯一工作区：DivineBeastsWorkspace｜正式引擎：UE5.8｜状态：分阶段实施中

**2026-10-18色彩设计输入**：十层地形、Road（道路）、PCG草簇、Nanite石崖、RVT融合和水岸生态在后续实际制作及标准审核地图中必须引用[SSLM全局美术配色策略V3.0](../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)，环境以明快、鲜绿、自然、通透为主，保留真实PBR；文档颜色为sRGB屏幕参考而非可直接覆盖的UE基础色。本计划既有Gaea/PCG工程门禁与阶段状态不因引用改变。

## P0–P7实际执行状态更新（2026-10-11）

本节按原生UE资产、进程退出码与日志计数，优先于下文初始待执行说明。

- P0完成：审查三层插件与PCG Schema v1/金标准资产；保留正式Village桥接材质，真实Gaea 2.3源工程/出口暂未找到。
- P1技术校准完成：50份PNG源图（30 PBR+1份1009平方16位高程+9掩码+10权重），SHA和纹理质量检查通过，全部1,018,081像素的十层权重总和为255。不是最终3A扫描贴图，不是Gaea真实输出。
- P2技术资产完成：49个UE Texture2D、146节点十层UMaterial及MI在UE5.8真实保存和独立回读；AssetRegistry确认母材质无直接Surface/VFX客户端硬依赖。**十份真实ULandscapeLayerInfoObject已全部创建并通过独立UE5.8反射验证**，LS_名称和FinalWeightBlending均正确。本工单通过隔离后缀Editor DLL执行、原DLL及模块清单已恢复且核验未改变；普通Editor模块正式重链接仍须待并发环境释放。详见Docs/Implementation/Terrain10LayerInfosCreation_20261011.md。
- P3校准投影完成，正式Gaea导入阻断：Gaea版本/坐标/SHA验证器、10张R8/1张little endian R16、1024条PCG Schema v1候选点及非法输入拒绝已完成。无真实Gaea 2.3出口，不能宣称生产导入。ALandscape::Import尚未在更新Editor DLL下执行。
- P4部分完成：原有PCG放置器复用生成4份真实项目Blueprint；2节点4边的GetLandscape→SurfaceSampler PCG输入Graph已保存并跨进程回读。新增GamePlatformPCGEditor地形权重过滤Graph通用Commandlet源码；UHT编译、真实Biome图、PCG Spawn/Bake均待完成。
- P5尚未完成：1009原生高程/权重清单已备好；独立L_DBA_TerrainMaskReview.umap尚未创建；正式Village不受校准高程影响。
- P6部分测试：源图QA、7组非法输入、Shader/UE资产类型/依赖检查与Surface C++编译通过；本轮增加十份真实LayerInfo的原生命令创建、隔离模块链接和独立UE反射回读成功证据。常规Editor目标完整重链接、QA地图、PCG运行、Client/Server新Cook均未取得本工单完整成功证据。受控RunTerrainGaeaFullReview.ps1已通过DryRun及并发安全拒绝，尚未运行其完整Apply流程。
- P7待人工签收：无十层Landscape/PCG实际融合画面与本工单GPU Profile，不得宣布最终完成。

证据位于Game/Saved/Logs/TerrainTextureImport20261011.log、TerrainMultiLayerAuthor20261011.log、TerrainMultiLayerUEAssetAudit20261011.log、TerrainPCGReviewBlueprints20261011.log、TerrainPCGLandscapeSampling20261011.log、TerrainPCGBlueprintGraphAudit20261011.log，及Saved/Validation/TerrainGaea。接续顺序：共享编辑器锁解除→Editor DLL正式链接→10 LayerInfo→4张权重过滤Graph→独立Landscape地图→PCG运行/Bake→三目标构建与双Cook→GPU与人工审核。


## 一、目标与工程边界

将Gaea真实高程及掩码来源标准化，映射为10种基础地表权重及原有PCG Schema v1地貌语义，再用UE5.8的Landscape多层母材质与PCG编辑器生成可复用场景。风格定位为《魔兽世界》式清晰的地貌区块、自然造型和区域辨识度，配合精细但克制、低颗粒感的电影级PBR地表。必须能够在不同项目、不同生物群系中复用平台算法。

- 平台GamePlatformWorldEditor：高程/掩码坐标与导入校验。GamePlatformSurfaceEditor：通用Material函数、LayerBlend和材质制作合同。GamePlatformPCG/PCGEditor：复用现有Schema、Biome、Layer、PriorityCarve、WorldDirector与Gold Level。GamePlatformData统一资源身份与异步租约。禁止新建ProjectPCG或平行插件。
- MobaCommon没有Gaea/地形材质职责，不机械建立空层。
- 项目DBAWorldPack_Village持有专属高程/掩码、材质实例/基础贴图和世界地图。DBAWorlds只装配，DBAClient只作雪、湿润及高质量纹理装配。其他世界使用同样的平台规则、不同内容包。
- Village正式地图同时用于Client和Dedicated Server；共享地形资产不得硬引用纯客户端Surface、VFX或高品质装饰资源。现有M_DBA_Village_LandscapeBridge、动态MID雪层与登录到Village链路均不得删除或直接覆盖。采用独立开发测试地图/资产验证。
- 源材质与测试Gaea风格高度图是程序化校准资源，不可虚称真正从Gaea 2.3导出。当前工作区搜索未发现真实Gaea项目/掩码出口：正式Gaea输入需待真实文件和指纹后继续执行。
- 当前另一个会话正在编辑大量登录场景资产并运行UE编辑器；不可停止它、重置仓库、抢占其正在打开的地图、篡改未提交资产或无授权推送。
- 标准顺序：源纹理/地形与掩码 → UE真实纹理/材质/图层 → 导入工具及图层权重 → PCG图和蓝图及独立验证地图 → 编译自动化及双端Cook → 人工最终审核。所有uasset/umap只能经UE Editor创建、保存与回读。

## 二、固定输入/输出规范

10基础层身份：Grass（青绿草）、ForestSoil（森林腐殖土）、Dirt（普通泥土）、Rock（山体石）、Scree（碎石坡）、Sand（湖岸沙）、Gravel（砾石）、RoadDirt（压实土路）、FarmSoil（耕作土）、ShoreMud（岸湿泥）。全项目10层不意味着单Landscape组件强制激活10层，典型组件约3~5层是待性能验证的初始目标。

Gaea Source Terrain Package：16位Height；语义Mask最少Slope、Erosion、Deposition、RockExposure、Flow、Shore、Forest、Buildable，必要的Exclusion和Curvature按需；统一原点、X/Y世界范围、cm标高/缩放、轴向翻转、Tile索引、边界重叠、源版本、SHA256。Height可导出原样16位，10个Landscape权重经统一算法算出每像素严格合计255；非法权重回落到Dirt=255，禁止全零黑洞。

Landscape采用WeightBlend为底，局部允许HeightBlend作细节融合；不能所有基础层都依赖HeightBlend。Triplanar/复杂噪声只应用于必要悬崖/高质量档。雪、苔藓、湿润、落叶属于覆盖层，不重复创造十套天气母材质。PCG用已登记Pcg.Biome.Id/Weight、Pcg.Rule.Slope/Height/Wetness、Pcg.Exclude.Mask、Pcg.Exec.Seed等字段，地图道路按既有优先级超过乔木/地被。

## 三、P0–P7分步工单与真实门禁

### P0 资产与架构基线
盘点已有GamePlatformSurface材质函数、GamePlatformPCG金标准真实模板/定义/地图、现有Village地形材质、源码与Cook边界。锁定项目已有AGENTS.md及插件规范，不改已有项目反射身份。把实际Gaea文件缺失、并发编辑器状态记录清楚。
门禁：实际路径、分层单向依赖及服务器权威边界能复核。

### P1 优先制作源纹理和校准地貌资源
按十基础层生成10×3组技术PBR贴图：BaseColor（sRGB）、Normal（线性）、ORM（R遮蔽G粗糙B金属度）。1024x1024可平铺、宏观色块完整、避免尖锐高频噪声。生成一份可导入测试的1009x1009 Height 16位和9种中立地貌Mask（不是实际Gaea输出），导出10张8位Landscape权重图，每像素权重合计255。附材质调色表、SOURCE_MANIFEST及SHA256。重运行不得覆盖不同内容。
门禁：真实PNG非空、RGBA/16位/8位正确、无缝纹理和权重归一校验，通过后才实施引擎资产导入。
工具：Tools/Unreal/Terrain下独占新脚本；项目内容归DBAWorldPack_Village/SourceArt/Terrain。

### P2 原生UE Texture2D、材质、LayerInfo资源
经锁定UE5.8编辑器导入P1纹理，创建项目技术多层Landscape母材质和10张真实LandscapeLayerInfoObject、用于QA的材质实例。平台仅拥有可复用材质作者规则/基础函数，不把项目具体纹理放进Surface插件；项目资源不修改已运行的雪地材质路径。完整连接材质层、粗糙度、法线，执行Shader编译、保存回读、AssetRegistry依赖校验。不能把SourceArt PNG当作已制作Texture2D。
门禁：真实UClass检查、10个图层名一致、材质图连接和Shader0错误，证据保留。

### P3 Gaea实际导入转换与非破坏性编辑层
实现Gaea Export Manifest规范、路径和SHA256校验、16位高程精度、Tile拼接/世界单位cm、Y轴方向和权重输入检查；对现有非破坏性Landscape Edit Layers保留源Gaea、道路修整和人工覆盖分离。没有真实Gaea导出则只能以标记明确的Calibration样本创建QA地图，不能宣称生产Gaea导入完成。需要时使用UE原生Landscape导入API而非伪装导入完成。
门禁：原像素与最终UE位置、Mask权重及层Info数据一致；错误输入失败关闭；源资产保留不覆盖。

### P4 复用GamePlatformPCG绑定同一地貌语义
利用既有GamePlatformPCG双模块与Schema v1/DomainCatalog/现有SG_ProjectOnLandscape、TPL_BiomeGenerator、PriorityCarve，优先把已有Landscape LayerWeights作为PCG采样输入，映射Slope/Height/Wetness/Biome/Exclude到现有Pcg.*字段。项目差异通过BiomePreset、LayerDefinition与SpawnPolicy数据资产表达，不增并行服务；可由UE实际创建受控Graph Instance和BluePrint，保存/重启回读。
门禁：草地→植被、岩坡→散石、岸湿地→岸草、主道路→乔木排除；与现有Gold G01–G16结合执行，不能仅凭文件存在宣称全部通过。

### P5 第一张场景与项目视觉
先采用完全独立的L_DBA_TerrainMaskReview审查关卡而不是正式L_Village_Start；配置新地形多层、坡度/岸带/道路/林带过渡、PCG实例及可审查光照/摄像机。完成独立编辑器重开、截图、美术调整与Bake Manifest后才允许评审迁入桃林三岛正式世界。真正三岛Gaea地貌需要真实Gaea源包，不用校准高程替代。
门禁：可见场景中的材质无黑斑、UV拉伸、明显接缝；PCG道路与农田不重叠，地图可重开。

### P6 全面统一自动化、性能和双端
执行资源SHA/权重算法与错误输入测试、三层架构检查、UE材质Shader编译/PCG数据验证，Editor/Client/Server三目标构建、真实地图运行、两套独立Cook/Stage/IoStore与UnrealPak容器审计；Server无客户端纹理及纯装饰VFX，权威地形碰撞/导航来源一致。记录Shader采样和指令、PCG输出指纹/生成时间、GPU/内存而非虚构预算。
门禁：逐条真实日志及退出码0；某一阶段阻断不能改名成“通过”。

### P7 人工与性能验收
真实UE Editor/PIE打开QA地图，检查《魔兽世界》式美术区域辨识、地形高度坡度与道路、草/岩/泥/沙10层融合、三向投射与材质质感、雪/苔藓/湿润、PCG生境与道路排除，高中低档GPU时间、FPS、内存和地图重启；人工填写截图设备与缺陷及签字。自动截图或NullRHI均不能代替人工审核结论。
门禁：真人确认后才允许标记“全部完成”。

## 四、回退策略与当前状态

新SourceArt/测试资产只由本任务独占命名、全部可独立关闭；旧M_DBA_Village_LandscapeBridge、PCG Gold与雪系统保留可回退。已发布LayerInfo及Map名称不静默重排，未来材料更换要有Manifest版本迁移。P0已核对现有插件、源Gaea缺口和共享地图；P1~P7仍以当前实际工具结果逐项更新，不将待执行写成已完成。
