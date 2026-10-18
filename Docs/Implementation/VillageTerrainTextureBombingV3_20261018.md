# SSLM 新手村地形纹理轰炸与 V3.0 配色实施记录

> 日期：2026-10-18。适用 Unreal Engine 5.8（虚幻引擎）与唯一工作空间 DivineBeastsWorkspace。统一设计依据为 Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md。所有英文资源名在本文均按所在段落给出中文用途。本文不替代真实UE资产验收日志。

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
