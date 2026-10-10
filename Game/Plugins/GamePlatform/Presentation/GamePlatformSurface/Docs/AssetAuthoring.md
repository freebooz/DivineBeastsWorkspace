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

- `MF_GP_SlopeMask`：世界法线与 World Up（世界向上）的 Dot Product（点积），暴露坡度阈值和锐度。
- `MF_GP_HeightMask`：Absolute World Position Z（绝对世界位置高度）按起始高度与过渡范围归一化，单位厘米。
- `MF_GP_WorldNoise`：以世界坐标提供宏观变化，避免相邻网格重复；高频细节仍来自纹理。
- `MF_GP_SnowLayer`：`Slope × Height × Noise × LocalSnow × GP_Surface_GlobalSnowAmount`，至少混合 Base Color／Normal／Roughness。
- `MF_GP_MossLayer`：允许比积雪更陡的表面，使用宏观和微观噪声打破整片绿色。
- `MF_GP_WetnessLayer`：原表面光学修正，Base Color适度变暗、Roughness降低、Normal微平滑。
- `MF_GP_PuddleLayer`：接近平面和低洼遮罩；不能用纯视觉水面替代真实碰撞或水体。

World Position Offset（世界位置偏移）、三平面投射、复杂程序噪声等高成本能力必须由质量档位或 Static Switch（静态开关）裁剪。

## 项目资产归属

平台只放跨游戏通用默认资产。神兽联盟雪、苔藓、岩石、泥土、桃林和建筑纹理，以及 `MI_DBA_*` 材质实例，归对应 `DBAWorldPack_*`。二进制资产必须由 Unreal Editor 创建、保存和回读，禁止文本占位。

2026-10-10已有五张Surface源纹理（WetnessNoise、PuddleMask、SnowAlbedo、SnowNormal、SnowORM）位于`GamePlatformSurface/SourceArt/Weather`，由`Tools/Unreal/Weather/GenerateWeatherSourceTextures.py`及其Manifest校验。**源PNG不是Texture2D.uasset**：必须通过正式UE导入脚本、材质节点连接与Shader编译，配合MPC真实创建和Server Cook剥离后才可转为P5通过。

## 人工验收

真实材质至少验证 Shader 编译、坡度／高度边界、湿润粗糙度变化、积水合理分布、远近景稳定、Material Stats（材质统计）和 ProfileGPU；Client Cook 应包含使用资产，Server Cook 应剥离纯表现资产。
