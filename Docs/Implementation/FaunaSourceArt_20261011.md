# Fauna 源美术制作与导入合同

日期：2026-10-11。范围：蓝紫蝶 ButterflyAzure、银蓝鱼 FishSilver 的真实项目源美术。

## 一、所有权与本次交付

生成器：Tools/Blender/Fauna/GenerateFaunaModels.py。

资源根：Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Fauna。

本次生成两种单网格、单材质模型，两个物种独立的 BaseColor、Roughness、Normal 1024×1024 纹理，包含两独立源场景的 DBA_Fauna_Models.blend，以及四张真实 Cycles 预览。所有权归项目公共表现核心包，平台通用 VFX 不引用具体物种。源文件不会代替正式 UE 资产，源美术不会进入 Dedicated Server 资源。

FaunaSourceManifest.json 记录生成器哈希、工具版本、13个产物的路径/字节数/SHA256、物种部件、源与FBX重导入几何、线性RGBA多重集合哈希、UV/动画合同。清单不把自身哈希递归写入。

技术制作检查通过；humanArtAcceptance 始终为 pending。两物种的最终艺术质量、游戏灯光、水体材质下的可读性、近景形变和目标设备预算仍需实际 UE 验收。

## 二、真实执行环境与复用命令

本机工具是 C:/Program Files/Blender Foundation/Blender 5.2/blender.exe，实际运行版本 Blender 5.2.1 LTS，构建哈希 9e2066aef7ef。

从工作空间根执行：

    "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --factory-startup --python-exit-code 1 --python "E:/work/2026/DivineBeastsWorkspace/Tools/Blender/Fauna/GenerateFaunaModels.py" -- --texture-size 1024 --render

生成器从自身路径解析工作空间根；上述绝对路径只是本机已执行命令，不是代码中的固定个人路径。

- --texture-size 接受512或1024，默认1024。
- --render 创建两物种各两个角度的真实 Cycles PNG；四线程、32采样，独立后台进程完成后退出。
- 不带 --render 只生成模型、纹理、blend和相应清单。
- 只允许背景进程。前台 Blender 会明确拒绝运行，避免清空人工场景。
- 目标旧清单必须标明属于本生成器；否则拒绝覆盖。任务范围外的文件不处理。
- 关闭本工具自身 blend 自动备份，不生成未登记的 .blend1。
- 失败抛出异常并返回非零；不得用残留旧清单将失败的新运行标记通过。

本轮最终完整运行退出码0。只有 Blender 对 Material.use_nodes 和 World.use_nodes 的将来6.0弃用提示；当前锁定5.2.1中功能有效。

## 三、文件映射

| 类别 | ButterflyAzure | FishSilver |
|---|---|---|
| FBX | Meshes/SM_DBA_ButterflyAzure.fbx | Meshes/SM_DBA_FishSilver.fbx |
| 底色 | Textures/T_DBA_ButterflyAzure_BaseColor_Source.png | Textures/T_DBA_FishSilver_BaseColor_Source.png |
| 粗糙度 | Textures/T_DBA_ButterflyAzure_Roughness_Source.png | Textures/T_DBA_FishSilver_Roughness_Source.png |
| 切线法线 | Textures/T_DBA_ButterflyAzure_Normal_Source.png | Textures/T_DBA_FishSilver_Normal_Source.png |
| 正交预览 | Previews/DBA_ButterflyAzure_Top.png | Previews/DBA_FishSilver_Side.png |
| 斜视预览 | Previews/DBA_ButterflyAzure_Oblique.png | Previews/DBA_FishSilver_Oblique.png |

原生源工程：DBA_Fauna_Models.blend。场景 Source_ButterflyAzure 和 Source_FishSilver 分开保存，模型局部原点相同约定，纹理打包进blend。FBX只导出选中模型，不含相机、灯或第二物种。

## 四、实测几何

以下数字来自真实 Blender 网格和导出 FBX 的重新导入，不是设计目标的复述。

| 指标 | ButterflyAzure | FishSilver |
|---|---:|---:|
| 源顶点数 | 2321 | 2750 |
| 三角面数 | 4306 | 5138 |
| X总长（cm） | 9.9000 | 23.5992 |
| Y总宽（cm） | 12.6000 | 8.13437 |
| Z总高（cm） | 1.66485 | 10.15859 |
| 最大轴半尺寸（cm） | 6.3000 | 11.7996 |
| 退化三角形 | 0 | 0 |
| 重复或共面反向复制三角形 | 0 | 0 |
| 顶点色Alpha范围 | 0..1 | 0..1 |
| RGB部位色种数 | 8 | 16 |

源坐标单位为米：

- ButterflyAzure 最小坐标 [-0.0518000, -0.0630000, -0.00712537]，最大 [0.0472000, 0.0630000, 0.00952308]。
- FishSilver 最小坐标 [-0.14199218, -0.04067187, -0.04839844]，最大 [0.093999997, 0.04067187, 0.05318750]。

尺寸包含触角、足或鳍；鱼体本身的高度与总鳍高不是同一指标。ButterflyAzure 的X总长包含前后翼边界，不能把它当作身体长度。

全部模型 X 向前、Y 向左右、Z 向上。蝴蝶四片翅膀是真实薄翼轮廓，另有胸腹、头、双眼、触角棒端和六足。鱼有连续流线体、真实叉尾、背鳍、臀鳍、成对胸鳍/腹鳍、虹膜/瞳孔、鳃盖线和口缘。所有模型均不是盒子或贴图矩形替身。

## 五、顶点颜色、UV和形变

统一顶点颜色层名称为 FaunaColorMotionWeight，FLOAT_COLOR/CORNER。RGB表示部位色，A只表示动画形变权重。源材质使用 BaseColor纹理乘顶点RGB；不把A连接不透明度。

### 蝴蝶

- 默认整体翼面在XY平面展开，默认宏观翼角0度，没有预先旋成V形。
- 左右翼铰链沿X：Y=±0.0025m，Z=0.0012m。转成UE是Y=±0.25cm、Z=0.12cm。每顶点保留原X作为沿铰链的位置。
- 前翼基础Z=0.0018m，后翼基础Z=0.0009m，沿翼宽叠加不超过0.0012m的轻微拱度。前后翼关节处有真实高度差，不生成同位置双面复制三角形。
- 翼A = smoothstep(0.0025,0.018,abs(Y))。身体、眼、触角与足A=0。
- 局部翼旋转应使用两侧相反的转向；停栖翼角、飞行扑翼角由运行状态驱动，翼根权重平滑过渡。对已生成的形变差值，不再把世界锚点平移加进去。
- 翼UV：U=abs(Y)/0.067；V=0.15+(X+0.055)/0.105×0.78。两侧镜像共用图案。
- 身体、足和触角使用底部色带，V在0.025..0.07附近。各部位的深浅由顶点RGB区分。
- 底色具有蓝/青/紫变化、翅脉、轮廓深色边和浅色珠点；粗糙度与法线随翅脉和微鳞纹变化，不是单色占位纹理。

### 鱼

- 鼻端位于+X，尾鳍位于-X。尾鳍最远约X=-0.141992m，鼻端X=0.094m。
- 横向游动沿局部Y，不能沿世界固定Y或Z实现。
- A = smoothstep(0,1,(0.035-X)/0.155)。X≥0.035m的头部为0，X≤-0.120m的尾端为1。
- 可使用局部X决定传播相位，A控制鱼身/尾端侧摆幅度；眼睛与前头保持稳定。
- 身体U=0.02+0.96×((X+0.080)/0.170)；环向V=0.12+0.76×q，其中q为绕X轴的归一化角，生成器中Y=ry cos(2πq)、Z=rz sin(2πq)。
- 鳍使用底部V=0.018..0.083色带，径向拓扑与鳍条UV配合。眼部通过顶点颜色读取相同中性色带。
- 底色包含深蓝背部、银色侧面、浅腹、鳞片和侧线；粗糙度与法线有相应鳞纹及鳍条变化。

### UE5.8逐粒子空间变换建议

本机正式头文件位于 Engine/Source/Runtime/Engine/Public/Materials。

MaterialExpressionTransform 的正式向量空间包含 TRANSFORMSOURCE_Instance / TRANSFORM_Instance，对应 Instance & Particle Space；旧 ParticleWorld 已标记废弃。MaterialExpressionTransformPosition 同样提供 TRANSFORMPOSSOURCE_Instance，旧 Particle 已标记废弃。

建议节点链为：AbsoluteWorldPosition（排除全部材质偏移）→ TransformPosition(World→Instance) → 局部翼/尾形变Custom输出局部差值 → TransformVector(Instance→World) → WorldPositionOffset。

这样逐粒子MeshOrientation和Scale参与变换。普通对象Local空间不应被直接假定为每个Niagara网格粒子的实例空间。这里仅记录源码核对结果；真实材质图编译与运行由UE资产任务验证。

## 六、FBX与纹理导入合同

Blender导出：

- axis_forward="X"，axis_up="Z"。
- apply_unit_scale=True，apply_scale_options="FBX_SCALE_UNITS"。
- colors_type="LINEAR"，use_tspace=True。
- 不烘焙骨骼动画，不导出叶骨，不加重复反面。
- 模型为单材质；翼和鳍由双面材质显示背面。

UE应使用本机已修正的传统FBX导入合同：

- FbxFactory；仅作者进程设置 Interchange.FeatureFlags.Import.FBX 0。
- import_as_skeletal=False；import_uniform_scale=1.0。
- convert_scene=True；convert_scene_unit=True，实现米到厘米的单次换算。
- vertex_color_import_option=REPLACE；normal_import_method=IMPORT_NORMALS。
- import_materials=False；import_textures=False；auto_generate_collision=False。
- 导入后检查实际FbxStaticMeshImportData、UV、RGBA和厘米级bounds。
- 不再额外100倍缩放；不沿用旧鸟模型30..300cm的最大半尺寸门禁，本次物种尺寸更小。

纹理设置：

- BaseColor：sRGB=True。
- Roughness：sRGB=False，线性数据。
- Normal：sRGB=False、NormalMap压缩；源为OpenGL +Y切线法线，UE导入启用FlipGreenChannel=True。
- 源粗糙度和法线不是全平色，各通道变化统计保存在清单。
- Blender预览材质法线强度0.65；UE最终强度仍应在真实灯光与物种尺度下核对。

## 七、验证与限制

生成器完成以下真实检查后才写 technicalValidation=passed：

1. 源几何坐标有限，三角面积非零，UV在0..1，真实部位色与0..1动画权重存在。
2. 检查按几何位置排序的三角形，没有共面反向复制或重复面。
3. 真正导出FBX，再在独立临时Blender场景中重新导入。
4. 重导入恰好一个网格，三角形数相同、米制尺寸一致。
5. FBX重导入明确指定colors_type="LINEAR"；Blender默认SRGB不符合本源合同。
6. 对所有角点RGBA在8位有效精度下排序并计算多重集合哈希；源与重导入必须完全一致，不能仅以Alpha最大最小值替代色彩验证。
7. 渲染四张真实Cycles图。最初近距离照明过强，已通过降低源预览灯能量修正；没有反向修改原纹理颜色抵消过曝。
8. 保存真实blend、PNG、FBX和文件SHA256清单。

此检查不包括UE重导入、Niagara图编译、GPU数组绑定、蝶停栖/鱼群行为、WPO上下界、目标设备性能、LOD、Cook/Stage、最终人工艺术确认。物种单网格约4.3k/5.1k三角；群量与可见距离必须在实际项目中测量，不据此宣称512个近景个体已满足性能预算。

本子任务不commit/push。总体目录登记由同轮集成任务统一维护，避免并发改写总体规划文档。
## 八、Fauna逻辑定义发现与Cook边界增量

本节为同日源资产交付后的集成补充。只核对现有Data实现与配置，并修改必要的目录条目；UE作者进程、Definition保存、运行态加载与实际Cook由本轮集成任务统一执行。

### 8.1 已核实的逻辑身份发现链路

项目当前代码 `Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsFaunaCatalog.cpp` 第20至30行枚举两个正式物种，并将物种名转为小写形成下列逻辑身份。作者脚本保存DataAsset时必须使用同一值；资产文件名本身不能代替逻辑身份。

| 物种 | DataAsset.LogicalId规范字符串 | 主资产ID |
| --- | --- | --- |
| ButterflyAzure | presentation.dba.environment.fauna.butterflyazure@1 | GamePlatformDefinition:presentation.dba.environment.fauna.butterflyazure@1 |
| FishSilver | presentation.dba.environment.fauna.fishsilver@1 | GamePlatformDefinition:presentation.dba.environment.fauna.fishsilver@1 |

实际加载有两个独立检查：

1. `GamePlatformData/Private/Definitions/GamePlatformDefinitionBase.cpp` 第4至16行规定类型为GamePlatformDefinition，`GetPrimaryAssetId()` 从合法LogicalId构造身份，保存时通过 `GetAssetRegistryTags()` 写入 `GamePlatformLogicalId` 标签。此处路径均位于 `Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source` 下。
2. `GamePlatformData/Private/Validation/GamePlatformDefinitionValidation.cpp` 第14至34行等待AssetRegistry完成发现，递归查询UGamePlatformPrimaryDataAsset派生类，以标签解析匹配逻辑身份；零个源返回MissingDefinition，多个源返回DuplicateLogicalId。它没有按资产文件名猜测路径。
3. `GamePlatformData/Private/Loading/GamePlatformAssetManager.cpp` 第127至137行先完成上述唯一性检查，随后要求引擎 `GetPrimaryAssetPath(AssetId)` 非空且等于唯一源路径。仅让AssetRegistry看见uasset或仅添加AlwaysCook，不能替代AssetManager的主资产目录扫描映射。
4. `Game/Config/DefaultEngine.ini` 第10至11行使用既有 `/Script/GamePlatformData.GamePlatformAssetManager`。VFXWorldDefinition继承VFXDefinition，后者继承GamePlatformDefinitionBase，符合既有扫描基类；无需额外Fauna资产管理器或空壳内容插件。

因此对 `Game/Config/DefaultGame.ini` 的既有唯一 `GamePlatformDefinition` 扫描条目做单个目录增量：

```ini
(Path="/DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions")
```

它位于现有 `PrimaryAssetTypesToScan.Directories` 数组内，不另加第二个相同类型条目，不清空其他目录。原有 `bHasBlueprintClasses=False`、`bIsEditorOnly=False`、递归规则及 `CookRule=Unknown` 保持不变。该配置只是允许合法定义建立映射；客户端正式资源收录与服务器排除仍由目标Cook配置控制。

### 8.2 本次配置修改

| 配置 | 规则 | 本次新增虚拟目录 |
| --- | --- | --- |
| Game/Config/Custom/FrontEndClient/DefaultGame.ini | DirectoriesToAlwaysCook | /GamePlatformVFX/Environment/Fauna；/DBAPresentationPack_Core/VFX/Environment/Fauna |
| Game/Config/Custom/VillageDevelopmentClient/DefaultGame.ini | DirectoriesToAlwaysCook | /GamePlatformVFX/Environment/Fauna；/DBAPresentationPack_Core/VFX/Environment/Fauna |
| Game/Config/Custom/DedicatedServer/DefaultGame.ini | DirectoriesToNeverCook | /GamePlatformVFX/Environment/Fauna；/DBAPresentationPack_Core/VFX/Environment/Fauna；/Game/Development/Fauna |
| Game/Config/DefaultGame.ini | 既有GamePlatformDefinition扫描目录 | /DBAPresentationPack_Core/VFX/Environment/Fauna/Definitions |

Fauna正式资源根包括平台通用模板，以及项目Definitions、Niagara、Materials、Textures、Meshes和Blueprints。源blend/FBX/PNG仍保存在内容包SourceArt下，不作为UE虚拟内容目录收录。开发审核地图位于单独的 `/Game/Development/Fauna`，没有写入任一客户端AlwaysCook列表；专用服务器明确NeverCook该目录。

原有BirdFlock目录细分、AmbientNature、天气、地表、UI、技能和英雄资源条目均保留。既有DBAPresentationPack_Core内容包继续承载第三层视觉资产，没有创建新的plugin或registry条目。

### 8.3 编辑守卫与验证范围

FrontEndClient、DedicatedServer和DefaultGame使用canonical `edit_project_files`，每个文件均以 `read_files.read_revision` 守卫做精确替换。VillageDevelopmentClient原文件已混合56处CRLF和46处独立LF；canonical工具明确拒绝混合换行并原子回退，整批没有部分写入。

得到本轮集成负责者的明确授权后，Village配置使用预置原始SHA256、唯一BirdFlock/Blueprints锚点和 `FileShare.None` 排他锁完成最小字节插入，仅加入249字节。去掉该插入段后的字节哈希严格等于原始文件，保留所有原字节与既有混合换行：

- 原SHA256：`26652680f0e3c251d308b7916064839c597e9ddfbf8fe15876317053d446db9a`。
- 修改后SHA256：`d4820e86cf9ab75380e822313849596a723d472e0d5476ede2fb877ba8cbce9c`。

本节静态验证包括目录条目准确且无重复、两个客户端均没有开发审核地图AlwaysCook、服务器三个Fauna排除目录、既有扫描类型和CookRule保持不变，并回读核对修改。此验证不代表新uasset已经由Data成功加载，也不代表Cook、Stage或服务器产物检查完成。集成验收仍需在读取最新配置的UE进程中核对两份实际Definition的LogicalId和标签、两个主资产ID的唯一路径、正式Data租约成功、客户端Cook收录，以及服务器Cook/Stage确实无Fauna视觉内容。

