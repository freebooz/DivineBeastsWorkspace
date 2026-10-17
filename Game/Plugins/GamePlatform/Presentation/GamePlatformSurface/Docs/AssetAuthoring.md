# GamePlatformSurface 材质资产制作规范

## 核心MPC

标准路径：`/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`。

模块编译后可用锁定 UE5.8 Editor Commandlet 创建真实资产：

```powershell
F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe Game\DivineBeastsArena.uproject -run=GamePlatformSurfaceCoreAssets -unattended -nop4
```

只验证：

```powershell
F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe Game\DivineBeastsArena.uproject -run=GamePlatformSurfaceCoreAssets -ValidateOnly -unattended -nop4
```

Commandlet拒绝覆盖已有资产；参数合同不匹配时返回失败并要求人工迁移。

## 材质函数实现原则

### 2026-10-17 新增通用四通道PBR覆盖函数

`MF_GP_PBRLayerBlend`（通用PBR属性覆盖函数）是新版本资产，保留旧有七个稳定函数原路径和接口。9个输入分别是BaseColor、OverlayColor（基础色与覆盖色）、BaseNormal、OverlayNormal（切线空间法线）、BaseRoughness、OverlayRoughness（粗糙度）、BaseAO、OverlayAO（环境遮蔽）、Coverage（覆盖因子0～1）；4个输出为BaseColor、Normal、Roughness、AO。以同一Coverage混合所有属性，Normal归一化、AO限制0～1。金属度对于普通土壤、岩石、积雪保持0，由项目母材质按需要独立约束；此通用函数不持有天气状态和项目纹理。

已通过`Tools/Unreal/Surface/AuthorTerrainPBRLayerBlendUE58.py`（平台UE5.8原生作者）创建20个实际表达式并保存回读，项目SSLM十层高精函数化候选已用三处函数调用消费。只有客户端才能持有纯表现高精材质；Dedicated Server（专用服务器）发布必须依Server Cook/UnrealPak审计，不能仅凭文件在平台目录推断隔离成功。


- `MF_GP_SlopeMask`：世界法线与 World Up（世界向上）的 Dot Product（点积），暴露坡度阈值和锐度。
- `MF_GP_HeightMask`：Absolute World Position Z（绝对世界位置高度）按起始高度与过渡范围归一化，单位厘米。
- `MF_GP_WorldNoise`：以世界坐标提供宏观变化，避免相邻网格重复；高频细节仍来自纹理。
- `MF_GP_SnowLayer`：`Slope × Height × Noise × LocalSnow × GP_Surface_GlobalSnowAmount`，至少混合 Base Color／Normal／Roughness。
- `MF_GP_MossLayer`：允许比积雪更陡的表面，使用宏观和微观噪声打破整片绿色。
- `MF_GP_WetnessLayer`：原表面光学修正，Base Color适度变暗、Roughness降低、Normal微平滑。
- `MF_GP_PuddleLayer`：接近平面和低洼遮罩；不能用纯视觉水面替代真实碰撞或水体。

World Position Offset（世界位置偏移）、三平面投射、复杂程序噪声等高成本能力必须由质量档位或 Static Switch（静态开关）裁剪。

## 项目资产归属

平台只放跨游戏通用默认资产。神兽联盟地图专属雪、苔藓、岩石、泥土、桃林和建筑纹理、MI实例归对应DBAWorldPack_*；跨地图通用的MI_DBA_Snow_Detailed归纯客户端DBAPresentationPack_Core，其父级M_GP_SnowCover_Detailed归GamePlatformSurface。二进制资产必须由Unreal Editor创建、保存和回读，禁止文本占位。

2026-10-10已有五张Surface源纹理（WetnessNoise、PuddleMask、SnowAlbedo、SnowNormal、SnowORM）位于`GamePlatformSurface/SourceArt/Weather`，由`Tools/Unreal/Weather/GenerateWeatherSourceTextures.py`及其Manifest校验。**源PNG不是Texture2D.uasset**：必须通过正式UE导入脚本、材质节点连接与Shader编译，配合MPC真实创建和Server Cook剥离后才可转为P5通过。

## 苔藓与雪的通用PBR组合（2026-10-12）

平台可选母材质 `M_GP_Surface_SnowMoss_PBR`（通用苔藓—积雪PBR母材质）通过 `Tools/Unreal/Surface/AuthorMossSnowPBRAssets.py`（UE5.8真实资产制作脚本）创作。新增五张通用苔藓源PNG及哈希清单由 `GenerateMossPBRSourceArt.py`（确定性源图工具）生成；不能用源图代替 Texture2D（纹理资产）。

材质覆盖顺序为基础岩石→宏观斑驳苔藓→湿润→积雪，统一混合基础色、切线空间法线、粗糙度、AO（环境遮蔽），金属度保持0。宏观覆盖采用世界空间坐标，坡度采用 VertexNormalWS（世界顶点法线）避免着色器反馈循环，`GlobalMossInfluence`、`GlobalSnowAmount`、`GlobalWetness` 均复用已有MPC稳定参数。第三层项目实例 `MI_DBA_MossSnow_Cliff`（神兽联盟岩壁场景风格）仅通过参数覆盖；正式Village服务器共享基础地形材质保持不变。

原有 `MF_GP_MossLayer`（简单苔藓覆盖函数）继续兼容旧材质；新写实组合方案独立发布，待真实UE编辑器着色器验证、Cook资产隔离和视觉签收后才可推广到全部世界。

## 人工验收

真实材质至少验证 Shader 编译、坡度／高度边界、湿润粗糙度变化、积水合理分布、远近景稳定、Material Stats（材质统计）和 ProfileGPU；Client Cook 应包含使用资产，Server Cook 应剥离纯表现资产。
