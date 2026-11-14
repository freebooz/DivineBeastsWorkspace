# SSLM L2 Painterly PBR 材质样板区实施规格

版本：V1.0｜日期：2026-11-11｜关联：SSLM全局美术艺术指导与视觉质量规范V4.0
状态：候选实施规格；等待L1光照/曝光基线批准后进入UE原生制作

## 1. 目标
建立一套可复用、可比较、可测量的艺术化PBR材质样板。在真实PBR响应基础上控制色块、粗糙度、法线和细节频率，使近景精致、中景自然、远景简洁。L2只负责材质表现和LookDev验证，不创建新的材质运行时架构，不复制天气、PCG、VFX或项目表现权威逻辑。

## 2. 前置条件
- L1必须先完成固定曝光、主光/天空补光和基础阴影的技术回读；L1尚未人工批准时，L2只能使用Candidate临时灯光，不得宣布材质正式通过。
- 复用现有材质资产注册表、命名策略、三层所有权、材质层覆盖物理规则和PBR验证流程。
- 开始前重新读取UE5.8实际AssetRegistry和现有材质清单；不能根据旧JSON或文档推断资产仍存在。
- 既有正式材质、七个稳定Surface材质函数、两个母版和MPC路径/参数为冻结合同；不重命名、不覆盖、不迁移。

## 3. 材质样板资产组
1. 草地/草甸：短草、湿草、枯草，尽量共享基础逻辑。
2. 泥土/道路：干土、湿土、压实道路，突出颗粒尺度和接触关系。
3. 普通岩石：花岗岩/风化石，表现大块体面、裂纹和中尺度颗粒。
4. 峭壁岩层：层理、断裂、侵蚀与沉积变化，重点检查斜光下的体积塑造。
5. 木材/建筑木：干燥旧木、受潮木材，突出木纹方向与粗糙度。
6. 竹材：竹节凸凹、纵向纤维、蜡质高光和新旧差异。
7. 树皮/叶片：树皮粗糙度与叶片双面/透光；不以强饱和色或强Subsurface掩盖形态。
8. 金属：铁件、铜件/装饰金属，避免全部做成镜面。
9. 湿润表面：湿岩、湿木、泥水边缘，雨水只影响暴露区域。
10. 雪地/霜：蓬松新雪、压实雪、冰霜分开验证，不把雪层等同于白色涂层。

## 4. 资产归属与架构边界
- 通用着色函数只进入现有GamePlatformSurface职责；优先复用原生表达式、已存在MF和母版，不新建平行材质插件。
- 项目专属纹理、材质实例归属于对应DBAWorldPack内容包；项目材质实例不得被平台插件硬引用。
- UI材质归GamePlatformUI，Niagara特效材质归GamePlatformVFX，不能为统一材质库而强行合并不同Material Domain。
- 天气数值仍由GamePlatformWeather权威状态提供；材质只消费客户端可视值，不决定服务器雨雪、摩擦、碰撞或水深。
- 新路径先标ProposedAssetPath；真实UE创建、Shader编译、保存冷重载和引用检查后才能提升为稳定路径。

## 5. Painterly PBR制作原则
### 5.1 大/中/小三级细节
- 大尺度决定材质整体色块、地表明暗和主要风化区域；应在中远景仍然成立。
- 中尺度决定岩层、木纹、竹节、泥土团块、草丛分布等可识别结构。
- 小尺度只补充微法线、细颗粒和微粗糙度；不能在远景形成噪点、闪烁或过强锐化。
- 同一材质检查近、中、远三档镜头；远景逐渐弱化小尺度细节，而不是简单降低纹理分辨率。

### 5.2 PBR通道合同
- BaseColor仅表示固有反射颜色；不得烘焙灯光阴影、AO、镜面高光或曝光效果。
- Normal按项目统一切线空间约定与UE法线压缩；数据纹理关闭sRGB；检查绿色通道方向和强度。
- ORM统一约定R=AO、G=Roughness、B=Metallic；数据纹理关闭sRGB。源资产通道不同则导入时显式重映射并登记。
- Roughness控制微表面散射；木材、岩石、湿表面、金属应有材质差异，禁止全场景共用单一极低粗糙度。
- Metallic仅真实导体区域使用；泥土、木材、草、石材通常保持非金属。
- Height/Displacement只有确需真实轮廓变化且预算允许时使用；优先用几何/Nanite解决大轮廓。

### 5.3 色彩与曝光
- 继承V3色板语义，不直接把sRGB HEX抄入线性BaseColor；必须进行sRGB到线性空间转换，并在L1固定曝光环境下人工校准。
- 新增颜色只记录候选值、色彩空间、纹理来源、适用区域和审核状态；不得未经审核登记为正式色值。
- 禁止用全屏Tint、过曝、强Bloom或浓雾修正材质自身的色彩空间/通道错误。

### 5.4 覆盖层物理关系
沿用Docs/Implementation/Materials/MaterialLayerCompositionPolicy.md：基础表面、苔藓/地衣、湿润、雪/霜/冰、积水按语义遮罩和遮蔽关系混合；法线必须采用正确归一化/重定向混合，不能直接线性叠加。雨水只作用于暴露表面，室内/檐下/洞穴需有遮蔽规则。

## 6. LookDev测试场景与固定镜头
优先在Lighting Lab开发目录内建立隔离测试区；若UE原生关卡尚未创建，先形成Editor/Monolith制作任务，不创建伪造资产。
- 测试网格：标准球体、平面、圆柱、立方体、斜面岩块、弯曲木板、竹竿段、叶片/叶簇、Landscape小样。
- M01 Material Overview：所有材质球与名称/版本/状态。
- M02 Grazing Light：低角度斜光，检查法线、粗糙度和细节闪烁。
- M03 Soft Diffuse：漫射柔光，检查色块和明度关系。
- M04 Wet/Dry Pair：同资产湿润/干燥并排对照。
- M05 Near/Mid/Far：同一材质在近、中、远视距的细节频率。
- M06 Landscape Blend：草地/泥土/岩石/道路的混合边界。
- M07 Village Cliff：峭壁岩层与山腰地表，使用Village专属参考基准。
- M08 Combat Camera：实际游戏相机高度，检查材质对英雄/技能/敌人的干扰。
所有镜头记录UE版本、地图路径、相机/FOV、分辨率、画质、RHI、天气、曝光、L1光照Profile和材质版本。

## 7. 技术验证
1. AssetRegistry检查资产真实存在、原生类型、归属包、引用者和重复资产。
2. Texture源文件审计：尺寸、位深、通道、sRGB、压缩、Normal标志、源路径、许可证和哈希。
3. Material Graph检查：节点/函数引用、Material Domain、Blend Mode、Shading Model、Sampler和静态开关组合。
4. Shader编译：记录每个母材质/实例的编译状态和错误；脚本存在不等于Shader通过。
5. 保存后关闭/重启Editor，冷重载回读参数、图和引用。
6. Client Cook验证资源包含与加载；Dedicated Server Cook验证纯视觉资源剥离。
7. 固定场景ProfileGPU/Shader Complexity/Texture Streaming/显存采样；检查透明过绘和材质实例数量。
8. 在目标PC人工检查材质表现，记录通过/有条件通过/不通过及修改原因。
沿用既有质量等级：High在预算内增加细节法线/RVT/局部覆盖；Medium减少昂贵采样但保留主要识别遮罩；Low使用Lite母版并简化透明/反射/位移。静态开关组合数量必须受控，最终成本以实际Cook和ProfileGPU为准。

## 8. L2验收门禁
- 资产路径和所有权符合三层架构；无平台到项目内容包的反向硬引用。
- BaseColor/Normal/ORM的色彩空间、通道与压缩正确。
- 近/中/远三档细节频率协调，远景不噪、不闪、不出现明显平铺。
- 斜光和柔光下粗糙度、法线、轮廓与材质类别可信。
- 湿润/雪/苔藓等覆盖层遵循物理语义和遮蔽关系。
- Shader编译、保存冷重载、Client Cook及Server剥离证据齐全。
- GPU、纹理内存、Shader复杂度和过绘达到项目批准的目标设备预算。
- 美术负责人签收材质观感；技术负责人签收资源/性能；玩法负责人确认战斗信息未受干扰。
未达到任一条件，状态保持candidate或rejected，不得接入正式场景作为最终材质标准。

## 9. L2交付物
- L2_MaterialAssetManifest.json：材质、纹理、来源、许可证、路径候选、状态和哈希。
- L2_TextureChannelAudit.json：纹理通道/色彩空间/压缩/Normal/尺寸审计。
- L2_LookDevCameraManifest.json：M01—M08固定机位合同。
- L2_MaterialReviewReport.json：Shader、Cook、ProfileGPU、内存、人工审核和缺陷。
- 原生材质/材质实例/纹理资产只能由UE5.8 Editor创建并保存；本规格不代表这些资产已生成。

## 10. 与既有工程资料的关系
- Docs/Implementation/Materials/MaterialNamingAndQualityPolicy.md
- Docs/Implementation/Materials/MaterialOwnershipAndContracts.md
- Docs/Implementation/Materials/MaterialLayerCompositionPolicy.md
- Docs/Implementation/Materials/MaterialValidationEvidence.md
- Docs/Implementation/Materials/MaterialAssetInventory_UE58.json（使用前重新扫描，不将历史盘点视为当前状态）
- Docs/Implementation/TerrainProductionPBRExecution_20261016.md（历史执行证据，区分已通过与待人工验收项）
- Tools/Unreal/Surface/现有作者与审核工具；优先复用，不新建重复作者链。

## 11. 当前状态
本文件是L2实施规格，不是UE原生资产。由于L1尚未在家中PC完成原生验证，所有材质视觉参数保持Candidate。允许先整理源纹理和资产清单、做离线通道审计、准备固定机位和测试用例；正式LookDev签收必须等待L1光照/曝光基线。