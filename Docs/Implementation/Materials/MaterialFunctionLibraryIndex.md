# 神兽联盟通用材质函数库索引台账 V1（201项）

**修订日期**：2026-10-13；**主工作空间**：DivineBeastsWorkspace（神兽联盟工作空间）；**静态基线**：`0a0cf10`。本台账记录设计归集，不是宣称201个材质函数已制作完成。真实 `.uasset`（虚幻二进制资源）状态需要 UE Asset Registry（资产注册表）与编译验证。

## 分类原则

- `ExistingContract`（既有稳定路径合同）：只冻结身份，不声称二进制资源存在。
- `NativeReuse`（引擎内置功能复用）：不得为凑数量复制节点。
- `MF`（材质函数候选）：拟新增或复用；建议路径不是实际资产路径。
- `ML`（材质层/混合层）：可以包含多个MF和PBR属性混合。
- `Other`（母版、实例、Niagara、UMG、后处理或编辑器）：优先由原负责系统承接，非强制函数化。

14类总数：**201项**，建议类型分布：`NativeReuse`（引擎内置）24项；`MF`（材质函数候选）98项；`ExistingContract`（既有合同）7项；`Other`（其他机制）60项；`ML`（材质层候选）12项。分类均为**初步**，须经过P1目标引擎节点可用性审核才能冻结。平台 → MOBA（按需消费） → 项目单向依赖，禁止基础层引用项目内容；项目技能仅为视觉分类，不恢复任何已取消的五行战斗权威规则。

## 201项唯一索引

### 01 基础算法与纹理处理（25项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-01-01 | 世界坐标纹理映射 | `WorldAlignedUV` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-02 | 三平面纹理映射 | `Triplanar` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_Triplanar`（仅建议） | 待 UE 实测 |
| MAT-01-03 | 纹理缩放旋转 | `UVTransform` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-04 | 远近纹理混合 | `NearFarTextureBlend` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_NearFarTextureBlend`（仅建议） | 待 UE 实测 |
| MAT-01-05 | 平铺纹理消重 | `TextureAntiTiling` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_TextureAntiTiling`（仅建议） | 待 UE 实测 |
| MAT-01-06 | 随机纹理偏移 | `RandomUVOffset` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_RandomUVOffset`（仅建议） | 待 UE 实测 |
| MAT-01-07 | 宏微观纹理混合 | `MacroMicroBlend` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_MacroMicroBlend`（仅建议） | 待 UE 实测 |
| MAT-01-08 | 图集区域采样 | `AtlasRegionSample` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-09 | 高度混合遮罩 | `HeightBlendMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_HeightBlendMask`（仅建议） | 待 UE 实测 |
| MAT-01-10 | 坡度遮罩 | `SlopeMask` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_SlopeMask`（既有合同） | 待 UE 实测 |
| MAT-01-11 | 海拔遮罩 | `AltitudeMask` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_HeightMask`（既有合同） | 待 UE 实测 |
| MAT-01-12 | 方向遮罩 | `DirectionMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_DirectionMask`（仅建议） | 待 UE 实测 |
| MAT-01-13 | 顶点颜色遮罩 | `VertexColorMask` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-14 | 曲率遮罩 | `CurvatureMask` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-15 | 接触区域遮罩 | `ContactMask` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-16 | 距离衰减遮罩 | `DistanceFadeMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_DistanceFadeMask`（仅建议） | 待 UE 实测 |
| MAT-01-17 | 噪声遮罩 | `NoiseMask` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Utility/MF_GP_WorldNoise`（既有合同） | 待 UE 实测 |
| MAT-01-18 | 软硬边缘过渡 | `EdgeSoftHardBlend` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_EdgeSoftHardBlend`（仅建议） | 待 UE 实测 |
| MAT-01-19 | 法线正确混合 | `NormalBlend` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-20 | 细节法线叠加 | `DetailNormal` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_DetailNormal`（仅建议） | 待 UE 实测 |
| MAT-01-21 | 粗糙度混合 | `RoughnessBlend` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-22 | 金属度混合 | `MetallicBlend` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-23 | 环境遮蔽混合 | `AOBlend` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-01-24 | PBR纹理通道解包 | `PackedPBRUnpack` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Core/MF_GP_PackedPBRUnpack`（仅建议） | 待 UE 实测 |
| MAT-01-25 | 材质属性统一混合 | `MaterialAttributesBlend` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |

### 02 天气季节与环境覆盖（16项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-02-01 | 积雪覆盖 | `SnowCoverage` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SnowLayer`（既有合同） | 待 UE 实测 |
| MAT-02-02 | 积雪厚度遮罩 | `SnowThicknessMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SnowThicknessMask`（仅建议） | 待 UE 实测 |
| MAT-02-03 | 迎风积雪 | `WindDrivenSnow` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_WindDrivenSnow`（仅建议） | 待 UE 实测 |
| MAT-02-04 | 融雪过渡 | `SnowMelting` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SnowMelting`（仅建议） | 待 UE 实测 |
| MAT-02-05 | 表面结霜 | `FrostSurface` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_FrostSurface`（仅建议） | 待 UE 实测 |
| MAT-02-06 | 冰层覆盖 | `IceCoverage` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_IceCoverage`（仅建议） | 待 UE 实测 |
| MAT-02-07 | 表面湿润 | `WetSurface` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_WetnessLayer`（既有合同） | 待 UE 实测 |
| MAT-02-08 | 雨滴附着 | `RaindropAdhesion` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_RaindropAdhesion`（仅建议） | 待 UE 实测 |
| MAT-02-09 | 积水形成 | `PuddleSurface` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PuddleLayer`（既有合同） | 待 UE 实测 |
| MAT-02-10 | 雨水流痕 | `RainStreaks` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_RainStreaks`（仅建议） | 待 UE 实测 |
| MAT-02-11 | 雨后干燥 | `RainDrying` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_RainDrying`（仅建议） | 待 UE 实测 |
| MAT-02-12 | 雨水泥泞 | `RainMud` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-02-13 | 灰尘堆积 | `DustAccumulation` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-02-14 | 风沙覆盖 | `SandAccumulation` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-02-15 | 火山灰覆盖 | `AshAccumulation` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-02-16 | 环境凝露 | `SurfaceCondensation` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SurfaceCondensation`（仅建议） | 待 UE 实测 |

### 03 自然生长侵蚀与附着（12项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-03-01 | 苔藓生长 | `MossGrowth` | 既有材质函数合同 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_MossLayer`（既有合同） | 待 UE 实测 |
| MAT-03-02 | 地衣覆盖 | `LichenCoverage` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-03-03 | 藻类生长 | `AlgaeGrowth` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-03-04 | 泥土附着 | `MudAdhesion` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-03-05 | 落叶堆积 | `FallenLeaves` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-03-06 | 草丛边缘覆盖 | `GrassEdgeMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_GrassEdgeMask`（仅建议） | 待 UE 实测 |
| MAT-03-07 | 岩石侵蚀 | `RockErosion` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_RockErosion`（仅建议） | 待 UE 实测 |
| MAT-03-08 | 土壤冲刷 | `SoilWash` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SoilWash`（仅建议） | 待 UE 实测 |
| MAT-03-09 | 树皮附生物 | `BarkEpiphytes` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-03-10 | 石缝植物遮罩 | `CrevicePlantMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_CrevicePlantMask`（仅建议） | 待 UE 实测 |
| MAT-03-11 | 潮湿区域微生物覆盖 | `MoistMicrobialLayer` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-03-12 | 自然沉积层 | `NaturalDeposition` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |

### 04 地形与开放世界融合（14项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-04-01 | 地形高度混合 | `LandscapeHeightBlend` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-04-02 | 坡度自动分层 | `LandscapeSlopeLayers` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeSlopeLayers`（仅建议） | 待 UE 实测 |
| MAT-04-03 | 海拔分层 | `LandscapeAltitudeLayers` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeAltitudeLayers`（仅建议） | 待 UE 实测 |
| MAT-04-04 | 悬崖三平面投射 | `CliffTriplanar` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_CliffTriplanar`（仅建议） | 待 UE 实测 |
| MAT-04-05 | 地形宏观颜色变化 | `LandscapeMacroColor` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeMacroColor`（仅建议） | 待 UE 实测 |
| MAT-04-06 | 距离细节控制 | `LandscapeDistanceDetail` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeDistanceDetail`（仅建议） | 待 UE 实测 |
| MAT-04-07 | 地形网格融合 | `LandscapeMeshBlend` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeMeshBlend`（仅建议） | 待 UE 实测 |
| MAT-04-08 | 道路地形融合 | `RoadLandscapeBlend` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_RoadLandscapeBlend`（仅建议） | 待 UE 实测 |
| MAT-04-09 | 河岸湿润过渡 | `ShoreWetnessTransition` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_ShoreWetnessTransition`（仅建议） | 待 UE 实测 |
| MAT-04-10 | 生物群落遮罩 | `LandscapeBiomeMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeBiomeMask`（仅建议） | 待 UE 实测 |
| MAT-04-11 | 地形侵蚀遮罩 | `LandscapeErosionMask` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeErosionMask`（仅建议） | 待 UE 实测 |
| MAT-04-12 | 地形层权重处理 | `LandscapeLayerWeights` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-04-13 | PCG分布遮罩 | `PCGDistributionMask` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-04-14 | 运行时虚拟纹理适配 | `LandscapeRVTAdapter` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeRVTAdapter`（仅建议） | 待 UE 实测 |

### 05 水体河流湖泊与瀑布（12项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-05-01 | 水面法线扰动 | `WaterNormalDistortion` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_WaterNormalDistortion`（仅建议） | 待 UE 实测 |
| MAT-05-02 | 水流方向映射 | `FlowDirectionMap` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_FlowDirectionMap`（仅建议） | 待 UE 实测 |
| MAT-05-03 | 水深颜色变化 | `WaterDepthColor` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_WaterDepthColor`（仅建议） | 待 UE 实测 |
| MAT-05-04 | 岸边泡沫 | `ShoreFoam` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_ShoreFoam`（仅建议） | 待 UE 实测 |
| MAT-05-05 | 波峰泡沫 | `CrestFoam` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_CrestFoam`（仅建议） | 待 UE 实测 |
| MAT-05-06 | 水下光衰减 | `UnderwaterLightAttenuation` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_UnderwaterLightAttenuation`（仅建议） | 待 UE 实测 |
| MAT-05-07 | 水体浑浊度 | `WaterTurbidity` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_WaterTurbidity`（仅建议） | 待 UE 实测 |
| MAT-05-08 | 水面涟漪 | `WaterRipples` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-05-09 | 焦散光纹 | `WaterCaustics` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-05-10 | 瀑布流动纹理 | `WaterfallScrollingUV` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_WaterfallScrollingUV`（仅建议） | 待 UE 实测 |
| MAT-05-11 | 流水透明度 | `WaterOpacity` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Water/MF_GP_WaterOpacity`（仅建议） | 待 UE 实测 |
| MAT-05-12 | 水面结冰过渡 | `WaterFreezing` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |

### 06 植被与生态（12项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-06-01 | 树干风动 | `TrunkWindMotion` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-06-02 | 树枝风动 | `BranchWindMotion` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-06-03 | 叶片细节风动 | `LeafWindMotion` | 原生表达式或已有内置函数 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-06-04 | 植被颜色随机 | `FoliageColorVariation` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_FoliageColorVariation`（仅建议） | 待 UE 实测 |
| MAT-06-05 | 叶片透光 | `LeafTransmittance` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-06-06 | 植物季节变色 | `FoliageSeasonColor` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_FoliageSeasonColor`（仅建议） | 待 UE 实测 |
| MAT-06-07 | 植被积雪 | `FoliageSnow` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_FoliageSnow`（仅建议） | 待 UE 实测 |
| MAT-06-08 | 植被湿润 | `FoliageWetness` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_FoliageWetness`（仅建议） | 待 UE 实测 |
| MAT-06-09 | 叶面结霜 | `LeafFrost` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_LeafFrost`（仅建议） | 待 UE 实测 |
| MAT-06-10 | 树皮苔藓 | `BarkMoss` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_BarkMoss`（仅建议） | 待 UE 实测 |
| MAT-06-11 | 角色触碰弯曲 | `FoliageInteractionBend` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-06-12 | 植物枯萎 | `FoliageWithering` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Foliage/MF_GP_FoliageWithering`（仅建议） | 待 UE 实测 |

### 07 建筑岩石木材与道具（16项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-07-01 | 边缘磨损 | `EdgeWear` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_EdgeWear`（仅建议） | 待 UE 实测 |
| MAT-07-02 | 表面划痕 | `SurfaceScratches` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_SurfaceScratches`（仅建议） | 待 UE 实测 |
| MAT-07-03 | 金属锈蚀 | `MetalRust` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_MetalRust`（仅建议） | 待 UE 实测 |
| MAT-07-04 | 铜绿氧化 | `CopperPatina` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_CopperPatina`（仅建议） | 待 UE 实测 |
| MAT-07-05 | 漆面脱落 | `PaintPeeling` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_PaintPeeling`（仅建议） | 待 UE 实测 |
| MAT-07-06 | 灰尘污渍 | `DirtStains` | 材质层与混合层 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-07-07 | 雨水侵蚀痕 | `RainErosionTraces` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_RainErosionTraces`（仅建议） | 待 UE 实测 |
| MAT-07-08 | 烟熏焦黑 | `ScorchChar` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_ScorchChar`（仅建议） | 待 UE 实测 |
| MAT-07-09 | 裂缝边缘 | `CrackEdgeBlend` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_CrackEdgeBlend`（仅建议） | 待 UE 实测 |
| MAT-07-10 | 木材年轮 | `WoodGrowthRings` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_WoodGrowthRings`（仅建议） | 待 UE 实测 |
| MAT-07-11 | 木纹细节 | `WoodGrainDetail` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_WoodGrainDetail`（仅建议） | 待 UE 实测 |
| MAT-07-12 | 石材微表面 | `StoneMicroSurface` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_StoneMicroSurface`（仅建议） | 待 UE 实测 |
| MAT-07-13 | 陶瓷釉面 | `CeramicGlaze` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-07-14 | 金属细节法线 | `MetalDetailNormals` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_MetalDetailNormals`（仅建议） | 待 UE 实测 |
| MAT-07-15 | 玉石内部色彩 | `JadeColorVariation` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_JadeColorVariation`（仅建议） | 待 UE 实测 |
| MAT-07-16 | 局部发光纹饰 | `LocalEmissivePattern` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Surface/MF_GP_LocalEmissivePattern`（仅建议） | 待 UE 实测 |

### 08 角色服装与装备（12项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-08-01 | 皮肤颜色变化 | `SkinColorVariation` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-02 | 鳞片纹理混合 | `ScalePatternBlend` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-03 | 毛发色彩变化 | `FurColorVariation` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-04 | 皮革磨损 | `LeatherWear` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-05 | 布料纤维细节 | `FabricFiberDetail` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-06 | 衣物污垢 | `ClothingDirt` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-07 | 角色沾水 | `CharacterWetness` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-08 | 角色积雪 | `CharacterSnow` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-09 | 角色局部结冰 | `CharacterIce` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-10 | 受击闪光 | `CharacterHitFlash` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-11 | 魔纹发光 | `CharacterMagicEmissive` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-08-12 | 角色溶解 | `CharacterDissolve` | 材质函数/引擎函数核查 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |

### 09 VFX通用特效材质（20项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-09-01 | UV流动 | `FlowUV` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_FlowUV`（仅建议） | 待 UE 实测 |
| MAT-09-02 | UV旋转 | `RotateUV` | 原生表达式或已有内置函数 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-03 | 极坐标转换 | `PolarCoordinates` | 原生表达式或已有内置函数 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-04 | 序列帧图集动画 | `Flipbook` | 原生表达式或已有内置函数 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-05 | 速度方向拉伸 | `VelocityStretch` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-06 | 漩涡扭曲 | `VortexDistortion` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_VortexDistortion`（仅建议） | 待 UE 实测 |
| MAT-09-07 | 软粒子深度淡化 | `SoftParticle` | 原生表达式或已有内置函数 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-08 | 摄像机距离淡化 | `CameraDistanceFade` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_CameraDistanceFade`（仅建议） | 待 UE 实测 |
| MAT-09-09 | 噪声溶解 | `Dissolve` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_Dissolve`（仅建议） | 待 UE 实测 |
| MAT-09-10 | 边缘燃烧 | `BurningEdges` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_BurningEdges`（仅建议） | 待 UE 实测 |
| MAT-09-11 | 透明度曲线遮罩 | `OpacityCurveMask` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-12 | 菲涅耳边缘光 | `FresnelRim` | 原生表达式或已有内置函数 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-13 | 发光脉冲 | `EmissivePulse` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_EmissivePulse`（仅建议） | 待 UE 实测 |
| MAT-09-14 | 径向能量渐变 | `RadialEnergyGradient` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_RadialEnergyGradient`（仅建议） | 待 UE 实测 |
| MAT-09-15 | 颜色梯度映射 | `GradientColorMap` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_GradientColorMap`（仅建议） | 待 UE 实测 |
| MAT-09-16 | 冲击波环形遮罩 | `ShockwaveRingMask` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_ShockwaveRingMask`（仅建议） | 待 UE 实测 |
| MAT-09-17 | 场景深度融合 | `SceneDepthBlend` | 原生表达式或已有内置函数 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-18 | 特效法线扰动 | `VFXNormalDistortion` | 材质函数/引擎函数核查 | `GamePlatformVFX`（平台视觉特效插件） | `/GamePlatformVFX/Materials/Functions/MF_GP_VFX_VFXNormalDistortion`（仅建议） | 待 UE 实测 |
| MAT-09-19 | 热浪扭曲 | `HeatDistortion` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-09-20 | 粒子随机外观 | `ParticleRandomAppearance` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformVFX`（平台视觉特效插件） | 引擎/领域其他资源 | 待 UE 实测 |

### 10 技能元素与魔法表现（14项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-10-01 | 火焰湍流 | `FireTurbulence` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-02 | 寒冰晶体 | `IceCrystal` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-03 | 雷电能量纹路 | `LightningEnergy` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-04 | 水元素流动 | `WaterElement` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-05 | 风刃轨迹 | `WindBlade` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-06 | 土石能量裂纹 | `EarthCracks` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-07 | 木系生命能量 | `NatureEnergy` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-08 | 金系金属光辉 | `MetalRadiance` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-09 | 治疗光环 | `HealingAura` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-10 | 防护结界 | `ProtectiveBarrier` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-11 | 法阵符文 | `MagicRunes` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-12 | 技能蓄力 | `ChargeEnergy` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-13 | 命中冲击波 | `ImpactShockwave` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-10-14 | 能量消散 | `EnergyDispersal` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `DBAPresentationPack_Core`（神兽联盟公共表现内容包） | 引擎/领域其他资源 | 待 UE 实测 |

### 11 UI用户界面材质（18项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-11-01 | 圆角矩形 | `SDFRoundRect` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_SDFRoundRect`（仅建议） | 待 UE 实测 |
| MAT-11-02 | 渐变背景 | `GradientBackground` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_GradientBackground`（仅建议） | 待 UE 实测 |
| MAT-11-03 | 边框生成 | `BorderShape` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_BorderShape`（仅建议） | 待 UE 实测 |
| MAT-11-04 | 描边发光 | `OutlineGlow` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_OutlineGlow`（仅建议） | 待 UE 实测 |
| MAT-11-05 | 柔和阴影 | `SoftShadow` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformUI`（平台界面插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-11-06 | 按钮高光扫过 | `ButtonShimmer` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_ButtonShimmer`（仅建议） | 待 UE 实测 |
| MAT-11-07 | 图标染色 | `IconTint` | 原生表达式或已有内置函数 | `GamePlatformUI`（平台界面插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-11-08 | 图标遮罩 | `IconMask` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_IconMask`（仅建议） | 待 UE 实测 |
| MAT-11-09 | 径向冷却 | `RadialCooldown` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_RadialCooldown`（仅建议） | 待 UE 实测 |
| MAT-11-10 | 线性进度 | `LinearProgress` | 原生表达式或已有内置函数 | `GamePlatformUI`（平台界面插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-11-11 | 分段进度 | `SegmentedProgress` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformUI`（平台界面插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-11-12 | 血条颜色渐变 | `HealthGradient` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_HealthGradient`（仅建议） | 待 UE 实测 |
| MAT-11-13 | 能量条流动 | `EnergyBarFlow` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_EnergyBarFlow`（仅建议） | 待 UE 实测 |
| MAT-11-14 | 选中发光 | `SelectionGlow` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_SelectionGlow`（仅建议） | 待 UE 实测 |
| MAT-11-15 | 动态波纹 | `UIRipple` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_UIRipple`（仅建议） | 待 UE 实测 |
| MAT-11-16 | 高亮呼吸 | `HighlightBreath` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformUI`（平台界面插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-11-17 | 稀有度边框 | `RarityFrame` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformUI`（平台界面插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-11-18 | 界面溶解转场 | `UITransitionDissolve` | 材质函数/引擎函数核查 | `GamePlatformUI`（平台界面插件） | `/GamePlatformUI/Materials/Functions/MF_GP_UI_UITransitionDissolve`（仅建议） | 待 UE 实测 |

### 12 贴花与表面交互（10项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-12-01 | 弹孔与撞击 | `ImpactBulletMark` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-02 | 刀砍划痕 | `SlashMark` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-03 | 爆炸焦痕 | `ExplosionScorch` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-04 | 地面裂纹 | `GroundCracks` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-05 | 石材破损 | `StoneDamage` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-06 | 泥水飞溅 | `MudSplash` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-07 | 脚印 | `Footprint` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-08 | 轮胎痕迹 | `TireTrail` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-09 | 湿润轨迹 | `WetnessTrail` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurface`（平台环境表面材质插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-12-10 | 贴花渐隐 | `DecalFadeOut` | 材质函数/引擎函数核查 | `GamePlatformSurface`（平台环境表面材质插件） | `/GamePlatformSurface/MaterialFunctions/Decal/MF_GP_DecalFadeOut`（仅建议） | 待 UE 实测 |

### 13 后处理与镜头（12项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-13-01 | 屏幕暗角 | `Vignette` | 原生表达式或已有内置函数 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-02 | 饱和度变化 | `Saturation` | 原生表达式或已有内置函数 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-03 | 局部屏幕扭曲 | `LocalDistortion` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-04 | 屏幕能量脉冲 | `ScreenPulse` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-05 | 角色轮廓描边 | `CharacterOutline` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-06 | 目标高亮遮罩 | `TargetHighlight` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-07 | 受击屏幕闪光 | `ScreenHitFlash` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-08 | 水下扭曲 | `UnderwaterDistortion` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-09 | 寒霜镜头边缘 | `FrostScreenEdge` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-10 | 热浪效果 | `HeatHaze` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-11 | 焦散投影 | `ProjectedCaustics` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-13-12 | 树影投影 | `ProjectedFoliageShadow` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformPresentation`（平台统一表现插件） | 引擎/领域其他资源 | 待 UE 实测 |

### 14 材质调试与诊断（8项）

| 编号 | 中文能力名 | 英文能力键（中文释义见左） | 建议实现类型 | 所属插件（中文说明） | 建议/稳定路径 | 状态 |
|---|---|---|---|---|---|---|
| MAT-14-01 | UV检查 | `UVVisualization` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-02 | 纹理拉伸检查 | `TextureStretchCheck` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-03 | 法线方向检查 | `NormalDirectionCheck` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-04 | 顶点颜色显示 | `VertexColorDisplay` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-05 | 遮罩可视化 | `MaskVisualization` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-06 | PBR通道检查 | `PBRChannelCheck` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-07 | 材质层权重显示 | `MaterialLayerWeightDisplay` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |
| MAT-14-08 | 世界坐标与距离显示 | `WorldPositionDistanceDisplay` | 母版/实例/Niagara/UMG/编辑器/其他系统 | `GamePlatformSurfaceEditor`（平台表面编辑器模块） | 引擎/领域其他资源 | 待 UE 实测 |

## 关联文档与验证

此文档为`MaterialCapabilityRegistry_V1.json`（完整机器能力台账）的中文可读索引。另见`MaterialAssetInventory.json`（已有合同资产审计）、`MaterialLayerCompositionPolicy.md`（覆盖物理规则）、`MaterialValidationEvidence.md`（验证与阻断）及`MaterialMigrationAndRollback.md`（迁移回滚）。

只有同一条记录具备UE资产类型、连接图、编译、保存回读、客户端Cook、专用服务器剥离与人工视觉证据，才能标记生产可用。
