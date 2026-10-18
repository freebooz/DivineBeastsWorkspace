# SSLM 新手村地形纹理轰炸与 V3.0 配色实施记录

> 日期：2026-10-18。适用 Unreal Engine 5.8（虚幻引擎）与唯一工作空间 DivineBeastsWorkspace。统一设计依据为 Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md。所有英文资源名在本文均按所在段落给出中文用途。本文不替代真实UE资产验收日志。

## 2026-10-18 本次继续执行：正式默认草地与十层V3的UE收口证据

实际资产已在UE5.8保存并经独立编辑器进程重新加载：十层正式BaseColor（基础色）已10/10重导入并冷重载，Dirt（普通泥土）阻断解除；正式共享地形81个表达式与三通道Texture Bombing（纹理轰炸）原有节点不变，并新建真实256×256 sRGB Texture2D（草地默认基础色贴图） /DBAWorldPack_Village/Materials/Landscape/Defaults/T_DBA_Village_GroundAlbedoV3Default（新手村地表绿色共享默认纹理）。原来的两处VillageGroundAlbedoTexture（地面基础色纹理参数）已统一指向该默认纹理，便于直接在Editor（虚幻编辑器）预览；客户端依旧通过GamePlatformData（平台异步加载）世界资源租约以同名参数加载V3高清2K草地源，与同源Normal（法线）和ORM（遮蔽/粗糙度/金属度）相容。

证据：Game/Saved/Validation/VillageTerrainBombingV3/AssetProof.json（纹理轰炸UE报告，expressionCount=81、三通道、sharedV3FallbackTexture存在、coldVerifyPassed=true；gpuPassed/cookPassed/humanReviewPassed仍false）。Game/Saved/Validation/TerrainBombing_V3_20261018/VillageBombFallback_UEApply_Resume.log（绿色256纹理创建及共享母材质修改）与VillageBombFallback_UEColdVerify_Resume.log（另一UE进程冷重载）。新地图依赖复审日志Game/Saved/Validation/VillageTerrainBombingV3/VerifyNewGreenDefaultServerMap_UE.log返回VILLAGE_SNOW_BRIDGE_ASSET_AUDIT_PASS（新手村资产隔离通过）。回退备份含Saved/ArtReview/VillageTerrainBombingV3/BeforeFallback（绿色默认绑定前的母材质及地图SHA）及原Before纹理轰炸备份。正式Village地图文件未修改，不破坏地图、导航、碰撞、PCG或专服角色。

渲染证据严格按实际像素评价：Game/Saved/Validation/TerrainV3D3D12Probe_20261018（真实D3D12同帧三源探测）、TerrainV3D3D12ProbePreloaded_20261018（预加载地图三源探测）、TerrainV3ViewportTicked_20261018（普通UnrealEditor真实视口跨帧截图）均得到黑帧，像素有效性未通过；不能将生成PNG或RTX4090硬件存在视为可见视觉验收。常规GUI视口诊断启动的本任务专属进程已按身份安全清理，其他会话的UE编辑器未被终止。项目级真实Client/Server Cook、GPU毫秒/FPS、多人游戏客户端画面、人工美术签收仍未完成；Server Cook曾因另一Foundation构建正在使用引擎而明确返回NotExecuted，不能写为通过或Shader失败。下方旧的9/10及“256纹理未绑定”描述仅为历史快照，以本节为准。

## 2026-10-18 历史首次原生实施记录（新增绿色默认纹理前的状态）

**正式Village母材质已真实修改，不是仅生成制作脚本。** 本次在UE5.8实际项目对共享 `/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge` 原位增量编辑并保存。正式原始55表达式升级至**81表达式**，真实接入3条UE引擎原生 `Texture_Bombing`（纹理轰炸）函数调用，分别服务Ground BaseColor（地表基础色）、Normal（法线）和ORM（遮蔽/粗糙度/金属度）；统一UV、Tiling（平铺）和Offset（错位）并给法线输入启用Is Normalmap（法线翻转修正）。保留旧Snow（积雪）、Wetness（湿润）和动态MID材质参数。DryColor（干燥乘色）调为白色，不再覆盖V3自然鲜绿的源草地色。

**原生Shader及质量分级**：UE真实日志中三组QualitySwitch（画质切换）节点均报告`Default/Low/High/Medium/Epic`可接线；三组FeatureLevelSwitch（图形特性切换）均报告`ES2/ES3_1/SM5/SM6`等有效分支。低中画质和移动ES3_1走原样单次采样，高、史诗画质的PC路径走原生纹理轰炸；真实UE的 `recompile_material` 未返回错误信息。引擎原生资源保存日志为`Game/Saved/Validation/TerrainBombing_V3_20261018/VillageBombApplyUE.log`，包含`SSLM_V3_TEXTURE_BOMBING_UE_APPLY_SAVED exprCount=81 channels=3 mapUnchanged=true`成功标记。

**独立冷重载及地图依赖门禁已通过**：另一新UE5.8 Editor进程运行verify模式，`VillageBombColdVerifyUE.log`实际返回 `SSLM_V3_TEXTURE_BOMBING_UE_COLD_VERIFY_PASS 81`；随后由正式`VerifyVillageLandscapeBridge.py`（新手村共享地图依赖审计工具）打开真正`L_Village_Start`并成功返回`VILLAGE_SNOW_BRIDGE_ASSET_AUDIT_PASS`（验证日志`VillageBridgeServerIsolationUE.log`）。原共享母材质SHA由`0d2ffe32...`变为`5718c015...`，正式地图SHA继续为`d8ae2fdc...`，未改变地图、碰撞、导航或PCG。备份位于`Saved/ArtReview/VillageTerrainBombingV3/Before`，原生保存与冷重载报告位于`Game/Saved/Validation/VillageTerrainBombingV3/AssetProof.json`。项目地形轰炸原生证据门禁`Tests/Architecture/TestVillageTextureBombingV3.py --require-native`及已有雪层/三层集成检查均通过。

**明快V3颜色落地边界**：运行时高精草地BaseColor/Normal/ORM仍按现行`DivineBeastsVillageSnowWorldSubsystem`客户端世界租约软加载，基础色取V3候选2K扫描色（不是单色Tint）。本轮已实际从该V3扫描源降采样得到256×256初始sRGB草地绿色备用PNG（`SourceArt/Terrain/BombingV3/T_DBA_Village_GroundAlbedoV3Default_Source.png`，SHA256 `0cae6a1933a1034e4a60d873cbe5dc11bd37651e4958bf08f04996f87c495cde`），并通过生成清单复核。但向共享母材质绑定该备用色纹理时检测到**Windows独占文件锁**，因此编辑器静态预览颜色绑定**未完成**；其作者`Tools/Unreal/Terrain/AuthorVillageGroundV3FallbackUE58.py`及来源作者`GenerateVillageBombV3Fallback.ps1`已准备，不能因为源PNG生成而宣称新Texture2D.uasset已产生或绑定生效。其它用户正在使用的UE编辑器不强制关闭。

**尚未验收**：完整D3D12场景画面、六态天气实际可见取图与黑帧问题、移动端性能、Shader GPU开销、Client/Server Cook及人工前后美术签字仍未验证。本次母材质改造并**不等于正式Village地图已绘制全部十种地表Layer**，原10层纹理Dirt锁阻断归此前专项单独追踪。验证按“真实保存/独立UE重载/真实地图依赖”与“GPU性能/视觉签收”分层记录，不因前者通过而宣布全部完成。

## 一、现有实物及修改目标

真实新手村地图为 DBAWorldPack_Village/Maps/L_Village_Start（正式Village关卡）；正式地形母材质为 DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge（中立Landscape桥接母材质）。修改前通过真实UE5.8只读反射确认：母材质55个表达式，Ground三组采样分别为 VillageGroundAlbedoTexture（地表基础色）、VillageGroundNormalTexture（地表法线）及 VillageGroundORMTexture（遮蔽/粗糙度/金属度）。

工程已有 GamePlatformSurface/MF_GP_TextureAntiTiling（平台两颜色消重混合函数），但该函数不负责对纹理对象实施随机UV采样。UE5.8引擎自带 Texture_Bombing（原生纹理轰炸函数），经UE5.8实际反射证实具有 Texture Object（纹理对象）、UVs（采样坐标）、Tiling（平铺系数）、Offset（随机偏移强度）、Optional Heightmap（可选高度图）、Contrast（高度对比度）、Enable Height Lerp（高度融合开关）、Is Normalmap（法线标识）等输入，以及Result（结果）输出。

## 二、原生材质改造方案

保留原有世界空间GroundUV投射、法线与ORM对应扫描关系、雪层/湿润及天气动态数值绑定；对三组Ground采样分别新增UE原生Texture_Bombing调用，使用相同Tiling、Offset和UV布局，不引入第二套UV随机算法。

- Albedo（基础色）调用Texture_Bombing后继续乘既有干湿颜色因子，随后与原雪层基础色融合。真实V3草地色在客户端仍由GamePlatformData世界期软加载，不用项目品牌碧玉Tint染色整张地表。
- Normal（法线）把原生函数的Is Normalmap设为True（静态布尔），使纹理随机旋转/翻转不破坏法线方向；再归一化并与既有雪面法线混合。
- ORM（遮蔽/粗糙度/金属度）使用同一随机采样参数；保留G粗糙度和原有干湿系数，继续保留原地表和雪层PBR（基于物理的渲染）属性。
- Enable Height Lerp初版为False（禁用昂贵的额外高度混合采样），不从不存在的高度绑定伪造结果。后续如需高度融合必须另行评估性能。
- VillageGroundBombEnabled（轰炸静态开关）为全三通道共用控制语义；优先尝试QualitySwitch（画质切换）让Low/Medium（低/中）单采样、High/Epic（高/史诗）随机采样，以及FeatureLevelSwitch（硬件渲染级别切换）让ES3_1（移动端）单采样。是否真正接入以UE反射日志为准，不能仅凭设计文字声称生效。

参数初始建议：VillageGroundBombTiling=1.0（世界UV平铺不二次放大），VillageGroundBombOffset=0.55（随机偏移强度）。调节需在标准视角与固定曝光下验证接缝、重复平铺、颜色及性能。

## 三、继续采用此前确定的王者峡谷式明快自然配色

用户指定的是《王者荣耀》经典峡谷的明快、鲜绿、自然、通透的美术语言，不直接使用其贴图或未经验证的官方取色。草地初始屏幕sRGB参考：标准#6B9956、向阳#86AD62、嫩绿#A6BC76；道路/泥土按#B7A181和#A88F6D作为自然暖褐参考；岩石按#89988C和#A8A38C参考。全部参考色不能直接当成UE线性BaseColor参数。

已有十层地形V3 PBR派生贴图由原扫描基础色校准，保留纹理内部自然细节和Normal/ORM，正式客户端GroundAlbedo已经有V3软路径配置。通过白色DryColor（干燥基色乘色）防止旧深褐色二次染色，湿润仍保留自然适当变暗和粗糙度下降。桃花、湖水、古风木构仍由各自资源独立呈现。

## 四、架构、安全与资源隔离

由于Village地图供客户端/服务器共享，母材质本轮只复用引擎自带Texture_Bombing及同WorldPack原有中性默认纹理；不将GamePlatformSurface（平台仅客户端Surface函数）、DBAPresentationPack_Core（项目纯客户端表现资源）硬引用到共享地图。高精2K三通道仍由现有DivineBeastsVillageSnowWorldSubsystem（客户端地表/雪层子系统）经GamePlatformData（平台数据服务）按世界生命周期加载和释放；不新增Tick（每帧循环）、Weather（天气）权威或客户端地形碰撞。

本轮真实修改范围为已有Landscape母材质，不调整村庄地形高度、PCG（程序化内容生成）生态规则、道路权重、两个PlayerStart（出生点）及服务器导航。十种LandscapeLayer正式绘制和PCG融合并非由一次Ground纹理轰炸自动实现。

## 五、执行文件及验证等级

作者工具：Tools/Unreal/Terrain/AuthorVillageTextureBombingV3UE58.py（原生材质增量编辑，inspect/apply/verify三态）。审查工具：Tools/Unreal/Terrain/InspectVillageTextureBombingUE58.py（真实55节点拓扑）、InspectTerrainTextureBombingFunctionsUE58.py（原生及平台函数输入输出）。静态门禁：Tests/Architecture/TestVillageTextureBombingV3.py（色板、依赖及原生证据检查）。

执行前要求共享母材质SHA与已核实版本一致，原地图路径和未提交状态可审查，并于Saved/ArtReview/VillageTerrainBombingV3/Before保存原始地图与材质二进制快照及哈希。引擎材质必须实际生成节点、编译、保存；随后以独立UE进程运行verify，报告位于Game/Saved/Validation/VillageTerrainBombingV3/AssetProof.json。引擎进程退出0不代表Python编辑动作完成，必须检查日志中的成功标记、错误/Shader消息和报告哈希。

真正验收时，在正式L_Village_Start使用固定曝光及同视角审核轰炸开关关闭/开启前后效果，重点检查近景2K法线、ORM通道一致性、远景消重、雪苔湿润自然性、道路与岩壁过渡；分别对High/Epic和Low/移动端度量Shader采样量、GPU毫秒、FPS并审核Client/Server Cook资源剥离。六态D3D12截图此前存在全黑问题，该失败结果不能自动沿用为已通过。本文件与作者代码存在不代表GPU、正式Cook或人工美术验收通过。

## 六、真实状态声明

最终状态只以本次工具实际执行证据为准：修改前55节点及UE原生函数签名已查明；原位UE母材质写入必须以APPLY_SAVED及独立COLD_VERIFY_PASS为据；实时游戏画面、PC/移动端性能、自动截图和人工审批均须分别验证。没有相应证据的项目统一保持待验。
