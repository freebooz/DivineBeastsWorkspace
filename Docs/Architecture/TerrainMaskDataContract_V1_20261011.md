# TerrainMask Data Contract V1（地貌掩码数据合同）
2026-10-11｜平台GamePlatformWorldEditor / SurfaceEditor / PCGEditor协作；具体Gaea地形内容属于第三层项目世界包。

Gaea真源：必须由Gaea 2.3实际导出，以源文件SHA-256和版本明确识别；程序生成的技术图称Calibration，不得宣称真实Gaea。
高程：单通道16位PNG或明确endianness的R16；源Height通常按地图1009×1009校准，生产尺寸从UE Landscape组件参数推导。掩码：同坐标单通道8位（需要时保留16位源），V1九项必需Slope、Erosion、Deposition、RockExposure、Flow、Shore、Forest、Buildable、Exclusion；Curvature和其他Wetness潜在图为可选扩展，不得悄悄省略V1必需字段。
空间：worldOriginXcm/Ycm/Zcm、worldWidthCm/worldLengthCm、minHeightCm/maxHeightCm、axisX/axisY、flipX/flipY、TileX/TileY、overlapSamples必须显式。图像行向下、世界Y方向需明确，不猜测轴向。Hash、BuildVersion、坐标和图层映射均作为导入前审计证据。
Tile接缝门禁：Gaea 2.3多Tile导出必须有tileSamples、tileX/tileY、tileYDirection=image_down以及每Tile的16位Height和九张Mask原文件SHA。水平邻接比较右/左像素列，垂直邻接比较下/上像素行，默认原样逐像素严格一致；容差只能由人工批准。使用Tools/Unreal/Terrain/CheckGaeaTileSeams.py只读校验，尚无真实Gaea Tile可验收，现有仅算法SelfTest通过。
基础地表固定：Grass、ForestSoil、Dirt、Rock、Scree、Sand、Gravel、RoadDirt、FarmSoil、ShoreMud；每像素权重总和255、全部候选权重为零时归Dirt兜底，NaN/Inf与越界掩码必须拒绝，材质层的HeightBlend不等于16位世界高度。
同源语义允许Landscape/PCG分别使用：例如Slope影响岩石权重及树木过滤，Shore影响湿泥/芦苇，人工主路同时覆盖材质权重和排除树木。PCG继续采用现有Pcg.Biome.Id/Weight、Pcg.Rule.Slope/Height/Wetness、Pcg.Exclude.Mask与Pcg.Exec.Seed，不建立第二套字段真源。
服务端权威地图只硬引用WorldPack中的安全基础材质/Texture；客户端高级PBR及动态雪纹理应由客户端软装配、明确Cook配置和真实UnrealPak隔离证明。
