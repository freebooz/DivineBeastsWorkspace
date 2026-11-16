# 神兽联盟 UE5.8 材质系统实施报告 V1.0

2026-11-10，Asia/Shanghai。SSLM 正式工程 `Game/DivineBeastsArena.uproject`，引擎 `5.8.0-0+UE5`，工作树初始为 `main` 的 `57a139ec`，存在本任务之前的未提交修改。

## 工程与实施范围

Monolith 0.24.0 连接正式工程后，Asset Registry 已完成初始扫描，初始世界为 `/Temp/Untitled_1`。实际启用 Monolith、PythonScriptPlugin、PCG、Water、SpeedTreeImporter、GamePlatformSurface 和 GamePlatformWater。引擎使用 D3D12/SM6，验证机显卡为 RTX 4090。实验中确认 `r.VirtualTextures=1`，岩石测试网格启用 Nanite。

| 挂载点 | 材质 | 材质实例 | 普通材质函数 | MPC | PCG Graph |
|---|---:|---:|---:|---:|---:|
| `/Game` | 122 | 20 | 9 | 1 | 26 |
| `/DBAWorldPack_Village` | 18 | 35 | 0 | 0 | 11 |
| `/DBAFoliagePack_Core` | 5 | 18 | 3 | 1 | 0 |
| `/GamePlatformSurface` | 12 | 11 | 85 | 1 | 0 |
| `/DBAPresentationPack_Core` | 235 | 452 | 13 | 0 | 0 |

上表为创建前的原生盘点。新资产位于 `/DBAWorldPack_Village/Development/Terrain/Art`，使用现有开发地形目录的 `DirectoriesToNeverCook` 规则。独立地图以已有草绘制实验图为原生模板，五层权重及样板在新地图内保存。项目配色沿用 SSLM 全局美术配色 V3.0，颜色参数采用 UE 线性值。

复用了原 SpeedTree 风动图、现有岩壁三向世界投射、UE 原生 Water 母材质、OpenWorld 草花网格和生长品种，以及五类地表和木屋 PBR 贴图。项目专用函数在 Village 包中接入既有平台 PBR 覆盖、宏微观混合、植被偏色和 RVT 法线函数。通用实现继续使用原平台归属。正式 Village 地图、地形桥接材质、ST9 母材质、原湖河实例、模板地图及两个默认配置共九个文件通过前后 SHA-256 比较。

## 节点与默认参数

| 母材质 | 原生节点和连接 | 新增或主要默认值 |
|---|---|---|
| `M_Master_Environment` | 颜色和法线参数纹理、ORM 的 G→粗糙度和 R→AO，世界 XY 宏观取样，角度校正细节法线；湿润函数输出 Base Color、Normal、Roughness | UV `1`，粗糙度 `0.79`，ORM 粗糙度权重 `0.78`，宏观 `0.28`／`1600 cm`，细节 `0.21`／UV `4`，湿润覆盖 `0.76` |
| `M_Master_Foliage` | 复制 ST9 材质，Masked、Two Sided Foliage，原 Alpha 裁切和 WPO 维持接线。原颜色×实例偏色，原 Normal 加细节，透光遮罩×原透光图调整后进入 Subsurface Color。叶片样板通过静态开关选择独立法线 | `LeafRoughness=0.73`，原粗糙度权重 `0.65`，`TransmissionStrength=0.58`，透光乘色 `(0.96, 1.07, 0.87)`，细节 `0.12`，`UseLeafCardNormal=false` |
| `M_Master_Grass` | 保留平台风动、原草片裁切和草片着色。低频世界空间宏观颜色与实例随机色叠加。草地 ORM G 驱动粗糙度，切线空间细节混合后转换回原世界空间法线 | 粗糙度 `0.75`，纹理粗糙度权重 `0.20`，宏观 `0.30`／`2400 cm`，细节 `0.18`，`UseVendorGrassInteraction=false`。开启原供应商弯曲时，使用局部颜色和细节默认值以符合每材质两个 MPC 的上限 |
| `M_Master_Rock` | 复用世界三向颜色、法线和 ORM。顶点红色反相和坡度低频遮罩控制苔藓，顶点绿乘本地覆盖控制湿润。PBR 覆盖后接细节法线和湿润，岩脚可选择采样 Landscape RVT | 苔藓覆盖 `0.45`、坡面权重 `0.36`、贴图尺寸 `320 cm`、苔藓粗糙度 `0.86`，细节 `0.23`，湿润覆盖 `0.88`，RVT 默认关闭。`MI_Rock_LakeSide` 开启，融合高度 `105 cm`、强度 `0.65` |
| `M_Master_Landscape` | Grass、Dirt、Rock、Path、Farmland 分别使用颜色、法线、ORM。四条独立 LayerBlend 分别混合 Base Color、Normal、Roughness、AO；归一化本材质读取的五层权重，空地块回退 Dirt。三张可替换 Gaea 遮罩经静态开关混合岩层，湿润读取 Dirt、Path、Farmland 权重。LandscapeGrassOutput 接草和花，RVT 输出颜色、世界法线、粗糙度 | Gaea 开关默认关，坡度 `0.53`、侵蚀 `0.21`、高度 `0`。`GaeaHeightMask` 预留独立高度图输入，初始为零权重。花密度乘数 `0.18`，宏观 `0.29`／`2500 cm`，各层粗糙度乘数 `1` |
| `M_Master_Wood` | 使用现有木墙颜色和打包线性法线。Normal 进行有符号解码后角度校正叠加细节，木纹亮度以小幅度调制粗糙度。世界宏观变化和顶点 G 湿润支路与环境母材质同合同 | 顺纹 UV `(1, 1)`，粗糙度 `0.73`，木纹粗糙度权重 `0.35`，宏观 `0.21`／`1600 cm`，细节 `0.21`，湿润覆盖 `0.76` |
| `M_Master_Water` | 复制 `/Water/Materials/WaterSurface/Water_Material`。保留 Single Layer Water 输出、原 WPO、吸收、相位、岸深遮罩、河流和 WaterBody 索引逻辑。两个 Panner 的法线经角度校正叠加；SceneDepthWithoutWater 减 PixelDepth 后驱动深浅水散射系数 | 世界波纹尺度 `220`／`410 cm`，默认波纹强度 `0.18`，颜色过渡 `240 cm`。湖／河实例波纹 `0.14`／`0.22`，`Water Roughness` 为 `0.085`／`0.13`，原生静态参数从各自有效父链读取 |
| `M_Master_Magic` | 线性灰度符文纹理 R 通道×线性发光色×局部强度×MPC 环境发光，输出至 Emissive。玉底色仍走受光表面 | 玉底色 `(0.035, 0.17, 0.107)`，粗糙度 `0.34`，发光色 `(0.20, 0.78, 0.44)`，强度 `2.1`，实验玉石实例 `1.7` |

共享湿润以 `max(项目 GlobalWetness, 平台 GP_Surface_GlobalWetness, 局部湿润覆盖值)` 与每像素遮罩相乘。湿色系数默认 `0.69`，湿粗糙度 `0.31`，以干粗糙度为上限，法线只轻微平滑。项目 MPC 在各 World 有独立参数实例。

| `MPC_Environment` 参数 | 默认值 | 实际消费 |
|---|---:|---|
| `GlobalWetness` | 0 | 环境、木材、岩石、Landscape |
| `GrassColorVariation` | 0.15 | Grass 默认静态排列 |
| `FoliageColorVariation` | 0.12 | Foliage |
| `RockColorVariation` | 0.10 | Rock |
| `DetailIntensity` | 1.0 | 环境、木材、岩石、植被、Grass 默认静态排列 |
| `EnvironmentEmissive` | 1.0 | Magic |

| 项目 Material Function | 输入、处理和输出 |
|---|---|
| `MF_ColorVariation` | 对象位置稳定种子叠加 PerInstanceRandom，低／高色偏和强度进入平台植被变色函数 |
| `MF_MacroVariation` | 一个可替换的线性遮罩按世界厘米取样，复用平台 MacroMicroBlend，输出低频颜色变化 |
| `MF_RoughnessVariation` | 纹理样本与基础粗糙度插值，结果 Saturate |
| `MF_DetailNormal` | 基础和细节切线空间法线经强度处理、角度校正、归一化 |
| `MF_Wetness` | 湿润遮罩及全局强度控制 PBR 覆盖，输出颜色、粗糙度和法线 |
| `MF_FoliageTransmission` | 叶片颜色、透光遮罩、透光乘色和强度，输出 Subsurface Color 所需透光颜色 |
| `MF_Emissive` | 线性遮罩、线性 HDR 发光色和非负强度，输出局部发光 |

材质实例从相应母材质创建。Static Mesh 按材质槽绑定，SpeedTree 只替换叶片槽，树干与枝干继续用原槽。Landscape 绑定 `MI_Landscape_Lab`，为五个目标层分配对应 LayerInfo。Gaea 应按同一地形 XY 标定替换线性遮罩并单独启用；WaterBodyLake 和 WaterBodyRiver 分别绑定自己的水材质实例，并保留原生 WaterZone。环境或玩法不依赖实验地图。

## 验证、限制与回退

- 冷启动 PID `42916` 打开已保存实验图并回读 45 个 `.uasset`。8 个母材质的 PCD3D_SM6 Shader 全部就绪，失败 0。本轮最终日志中，实验材质的 Shader 警告 0，8 个母材质图回读的孤立表达式 0。7 个函数通过母材质调用链验证，22 个实例回读实际父材质和覆盖。湖实例与苔藓实例的末次调整另做了包重载。
- `GamePlatformNamingValidator` 曾产生 9 条独立内容命名诊断，要求 7 个 `MF_*` 和 `MPC_Environment` 改为 `M_`，并要求 `RVT_*` 改为 `T_`。保留任务指定和实际资产类型对应的命名。该诊断属于项目资产校验，另列于 Shader 统计之外。
- 五层中心回读权重依次为 `0.4471 / 0.4157 / 0.4118 / 0.4549 / 0.4667`，过渡点 Grass `0.1843` 与 Dirt `0.2314`。场景截图可区分不同地表。岩石试件使用启用 Nanite 的现有网格，固定曝光下有苔藓和湿润对照。
- 本地 PIE 回读原生湖、河动态实例及样条。湖为闭合 6 点，河为开放 4 点，采样水深约 `62 cm` 和 `150 cm`。湖实验实例采用 UE 原生深度、边界裁剪和 Water Info 开关，波纹使用材质双法线，保持原工程湖河材质的独立引用。苔藓样板的 `MossSlopeCoverage` 调为 `1.25`。
- 实验场景的显式 HISM 样板在 PIE 中保留 49 个草实例和 16 个花实例。编辑器和 PIE 回读 `t.MaxFPS=0`、`r.VSync=0`。
- RTX 4090、D3D12 下，`ProfileGPU` 的草区单帧为 `4.08 ms`，最终湖区镜头单帧为 `2.85 ms`。已拍摄 Shader Complexity；Water 母版的默认排列采样最多。下表为 Monolith 对母版当前平台的估计，真实排列随静态开关、地形组件权重和水体类型变化。GPU 数值只覆盖该测试机、独立地图和单帧。

| 母材质 | 已保存表达式 | 像素纹理采样估计 | 像素 Shader 指令估计 |
|---|---:|---:|---:|
| `M_Master_Environment` | 34 | 5 | 406 |
| `M_Master_Foliage` | 88 | 4 | 436 |
| `M_Master_Grass` | 69 | 7 | 489 |
| `M_Master_Rock` | 93 | 17 | 605 |
| `M_Master_Landscape` | 94 | 8 | 359 |
| `M_Master_Wood` | 45 | 4 | 401 |
| `M_Master_Water` | 96 | 61 | 1769 |
| `M_Master_Magic` | 8 | 1 | 346 |

本轮资产写入通过 Monolith 原生编辑器接口执行。四个图层名由既有 `GamePlatformSurfaceTerrainLayers` 命令工具生成，因为 UE 的编辑器 Python 将 LayerName 标记为只读。编辑器首次添加 Water 属性引脚触发了输入数组断言，水资产当时尚未保存。恢复后用 Monolith 复制完整属性节点并接回原位移输出。先后修正了湿度遮罩采样类型和草材质的 MPC 超额问题。

松针、毛竹叶、银杏叶和桃花使用项目现有贴图做叶片样板。樱花实例是桃花源图的透光调整样板。当前工程未找到黄山松、毛竹、银杏整株 SpeedTree 网格，动态风动、远近 LOD 与物种视觉签收应使用对应的原始整株模型复验。

- Landscape 的两个 GrassOutput 均已绑定，重存时 64/64 个 GrassMap 组件有效。默认配置、短时开启 RuntimeGeneration 和冷重载后的本地 PIE 都回读到 0 个自动草花实例。该验收项未通过。显式 HISM 样板独立证明了草花网格和材质的运行显示。
- 湖面已退出地形样板区，贴岸仍有折射断裂和陡坡截面。应在真实湖盆地形与 WaterZone 的交界复验深浅水、动态反射与岸线过渡。
- RVT 已写入实验地形并连接岩脚采样，Gaea 已接线性高度、坡度与侵蚀输入。目标地块的 Gaea 高程数据、RVT 接缝对照、风动与远景闪烁的连续镜头，以及人工视觉签收仍待专项复验。

所有正式引用继续使用原路径。回退时按下表定位本轮新增包，并保留原有提交及工作树修改。验证证据归工程 `Saved/Validation/MaterialLab_20261110`。此次交付保持实验目录归属，正式地图迁入、目标机 GPU 预算及发布 Cook 另行验证。

## 资产清单

| 类别 | UE 资产包路径 |
|---|---|
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Environment` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Foliage` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Grass` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Landscape` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Magic` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Rock` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Water` |
| 母材质 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Master/M_Master_Wood` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_ColorVariation` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_DetailNormal` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_Emissive` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_FoliageTransmission` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_MacroVariation` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_RoughnessVariation` |
| 材质函数 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Functions/MF_Wetness` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Foliage/MI_Foliage_Bamboo` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Foliage/MI_Foliage_Ginkgo` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Foliage/MI_Foliage_Peach` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Foliage/MI_Foliage_Pine` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Foliage/MI_Foliage_Sakura` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Foliage/MI_Foliage_Willow_ST9` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Grass/MI_Flowers_Meadow` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Grass/MI_Grass_Dark` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Grass/MI_Grass_Emerald` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Grass/MI_Grass_Meadow` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Landscape/MI_Landscape_Gaea` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Landscape/MI_Landscape_Lab` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Magic/MI_Magic_JadeRune` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Rock/MI_Rock_Cliff` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Rock/MI_Rock_LakeSide` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Rock/MI_Rock_Moss` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Rock/MI_Rock_Wet` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Water/MI_Water_Lake` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Water/MI_Water_River` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Wood/MI_Wood_Ancient` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Wood/MI_Wood_Dark` |
| 材质实例 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Instances/Wood/MI_Wood_Weathered` |
| 参数集合 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/ParameterCollections/MPC_Environment` |
| 地形草花类型 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/LGT_DBA_Village_Flowers` |
| 地形草花类型 | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/LGT_DBA_Village_Grass` |
| 地形 LayerInfo | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/LI_MaterialLab_Dirt` |
| 地形 LayerInfo | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/LI_MaterialLab_Farmland` |
| 地形 LayerInfo | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/LI_MaterialLab_Path` |
| 地形 LayerInfo | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/LI_MaterialLab_Rock` |
| RVT | `/DBAWorldPack_Village/Development/Terrain/Art/Materials/Test/MaterialTestAssets/RVT_DBA_MaterialLab` |
| 实验关卡 | `/DBAWorldPack_Village/Development/Terrain/Art/Maps/L_DBA_Material_Lab` |

Grass 层复用 `/Game/Development/Terrain/GrassPaint/LI_Grass`。

## 原生截图

固定曝光的 UE 原生视口截图。画面 `1408 × 792`，逐镜头任务完成后校验文件尺寸和 SHA-256。

| 验证内容 | 截图 |
|---|---|
| 实验关卡总览 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Overview.png) |
| 叶片透光和透明裁切 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Foliage.png) |
| SpeedTree 柳树 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_SpeedTree.png) |
| 草地实例差异与花样板 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Grass.png) |
| 岩石苔藓 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_RockMoss.png) |
| 湿岩 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_RockWet.png) |
| 五层 Landscape | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Landscape.png) |
| 原生湖面与岸边待修问题 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Lake.png) |
| 原生河流 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_River.png) |
| 木屋 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Wood.png) |
| 玉石符文 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_Jade.png) |
| Shader Complexity 总览 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_ShaderComplexity_Overview.png) |
| Shader Complexity 草地 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_ShaderComplexity_Grass.png) |
| Shader Complexity 湖泊 | [查看截图](../../../Saved/Validation/MaterialLab_20261110/Screenshots/SSLM_MaterialLab_ShaderComplexity_Lake.png) |

[原生资产、Shader、PIE、日志与图像哈希记录](../../../Saved/Validation/MaterialLab_20261110/SSLM_MaterialLab_NativeVerification.json)

## 正式新手村地形接入（2026-11-10）

通过 Monolith MCP 将五层地形 PBR 方案接入 `/DBAWorldPack_Village/Maps/L_Village_Start`，沿用 `M_DBA_Village_LandscapeBridge_Layout` 及其现有母材质作为正式入口。五层分别绑定已有的 `LS_Grass`、`LS_Dirt`、`LS_Rock`、`LS_RoadDirt` 和 `LS_FarmSoil`。空白权重区域由原草地纹理、低频宏观变化和坡面岩层渲染，新增绘制权重参与基础色、法线、粗糙度和 AO 混合。道路、湖岸、草花、湿润和积雪沿用正式遮罩和天气参数。

- 原生保存并重新载入地图与材质。256 个组件的材质实例均回读到启用状态，九处碰撞采样与基线相同。修改前后三份包哈希和同镜头截图已归档到 `Saved/Validation/VillageUnifiedTerrain_20261110`，其中 `Before` 存放原生包备份。
- 客户端和专服均通过 Cook/Stage 及容器门禁，三个正式地形包均在新产物中。客户端首次校验记录了一次着色器工作进程启动耗时警告，沿用相同参数重试通过。
- 本地专服新手村 PID 47152、主竞技场 PID 38896 已就绪。客户端 PID 44288，日志回读 	.MaxFPS=0 和 .VSync=0。新版新手村已记录成功准入，主竞技场准入及人工视觉签收待完成。
