# 《SSLM》Niagara方块粒子 Shader 隔离诊断与回归计划

更新：2026-10-11。工作空间：DivineBeastsWorkspace（SSLM工作空间），引擎：UE5.8，执行工具：HWebCodex。

## 1. 核实的问题

现有`Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/PearPBRMasksCorrect_1440.jpg`由真实 D3D12 离屏游戏截图获得，粒子显示为灰色棋盘格四边形，而非正确的梨花轮廓。Niagara 原系统近、远两层实际 SpriteRenderer 均有静态材质引用 `MI_DBA_Ambient_PearPetal`，材质父类为 `M_GP_AmbientNature_PBR_Foliage`，在 Unreal API 回读中启用了 Niagara Sprite、Masked、Two-Sided Foliage（双面植被）。这些资产检查通过仍不意味着 GPU 真正使用了正确的 Shader。

`PBRTextureSamplerDiagnostic.json`确认平台母材质中的 `LeafOpacity` 与 `LeafRoughnessMap` TextureSampleParameter2D 默认 `SAMPLERTYPE_COLOR`；项目真实 Mask 纹理用 `TC_MASKS`，`sRGB=False`。已有独立克隆将默认纹理和采样器修为 `SAMPLERTYPE_MASKS`，`PBRMaskSamplerProbeResult.json`记录编译接口无错误、两个渲染器绑定试验 MI；但最新截图仍有方块，原因尚未隔离，**不得直接修改全部正式母材质或认定已经消除问题**。

原始四通道梨花图位于 `Saved/Validation/FallingFoliage/PBRAtlas20261011/Stage/Maps/PearPetal/`，Opacity 是中央白色花瓣与黑色背景，并非整张不透明方形。需要在UE中验证 Texture2D 实际采样与 Shader 实际编译，而不能仅用源 PNG 推断。

## 2. 独立 GPU 验证计划

已新增 `Tools/Unreal/VFX/FallingFoliage/AuthorFoliageShaderIsolationUE58.py`。必须用已锁定的UE5.8及真实主工程执行；只允许新建 `/Game/Development/FallingFoliage/Probes/` 下的测试资产，无覆盖已有资产、正式平台模块或角色关卡。执行状态应以其输出的 `ShaderIsolationAudit.json` 和真实 D3D12 GPU 游戏截图为准。

四组对照采用已知可生成GPU粒子的 `NS_GP_Weather_Snow` 克隆发射器，消除原Ground Motion故障干扰：`Solid`（单色红色Unlit、Mask=1）辨认 Shader 回退；`UV0`（纹理坐标RG梯度、Mask=1）验证 SpriteUV0；`Opacity`（真实梨花Albedo+Opacity.R、Unlit、Masked）验证透明裁剪轮廓；`PBR`（已有真实项目材质实例与修正MaskSampler的副本）验证完整受光材质。四组复用一个独立游戏审查关卡，按空间分区进行GPU截图审核。

仅有如下全部成立才认定“方块已在独立试验中解决”：纯色显示为红色而非灰格、UV0有连续二维梯度、透明测试可见非矩形梨花轮廓、PBR可见正确受光的梨花；同时真实 GPU 日志无Shader/Niagara编译失败，并确保材质实例、遮罩通道及纹理坐标绑定在实际渲染器上。人工确认并记录独立地图GPU截图/日志（包括渲染设备、时间、分辨率、Shader平台）。Niagara保存和编译数据本身不得当作视觉签收。

## 3. 平台修复前置条件

独立 Shader 门禁未通过，不改正式 `M_GP_AmbientNature_PBR_Foliage` 或 `NM_GP_FoliageGroundMotion`；即使已有`RepairPBRMasterMaskSamplersUE58.py`和`GuardedRepairFoliageGroundMotionHLSL.ps1`也不提前运行。

只有在试验定位且修复可复现、确认原资产Git状态干净、存在逐资产SHA备份、无人并发编辑相同资源后，才修复平台通用母材质与粒子地面运动模块。平台修复需检查 `Particles.Age` 在 CustomHLSL 中是否存在有效ParameterMap输入，否则按既有 `NM_GP_FoliageGroundMotion_AgeSafeProbe` 修复以避免GPU编译失败。项目层保留各项目MI及Niagara外观，不将项目纹理写入平台层。MOBA层不创建纯装饰重复机制，Dedicated Server不携带纯视觉粒子。

## 4. 完整回归门禁

按“独立GPU场景 → 平台Niagara基础系统 → 项目近/远景梨花/桃花 → 其他15种自然飘落外观 → 前端角色工作室”依次验证。需要空中分布、真实半透明轮廓、落地停顿、地面滚动与滑行、阵风二次扬起、不穿插地形，以及无风/微风/阵风和高/低画质的性能。角色选择与角色创建都验证`L_DBA_CharacterStudio`场景，包括切换、卸载及UI无遮挡。全部需正式 Client D3D12 截图/视频/帧时间；正式 Editor、Client Build/Cook、服务器内容剥离为分别独立的门禁。

## 5. 当前任务状态

当前已完成：真实 Git/架构审查及 PBR 源图、遮罩采样器、Niagara 渲染器实例回读；新增两套隔离测试作者工具、运行 `AuthorFoliageShaderIsolationUE58.py` 并完成断点续建，在真实 UE5.8 下保存三套 Shader、三套 Niagara 系统和一张 Development Probe UM​​AP；在 D3D12 离屏游戏模式下获得 `Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/ShaderIsolationGPUReview_1440.jpg` 和 2560×1440 原图 `Game/Saved/Screenshots/WindowsEditor/HighresScreenshot00019.png`。**四组独立 Niagara 粒子仍呈灰色方块，说明问题范围包括最简纯色 Shader，不能仅修 Mask/UV/PBR。**

## 6. 2026-10-11 23时 GPU回归追踪

- `Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/ShaderIsolationAudit.json` 记录独立 `Solid`、`UV0`、`Opacity` 系统各两个 SpriteRenderer，资源创建成功，GPU 签收仍为 `false`。第一次作者运行曾因UE MaterialEditingLibrary不能连接测试脚本写的 `Coordinates` 输入引脚而中止；已纠正为TextureSample默认UV0，并通过精确的四资源SHA断点续建完成测试资源。
- D3D12截图中纯色红色Shader对应的粒子仍为灰片，表明默认/回退材质或SpriteRenderer实际Shader使用路径值得优先调查；这不是材料引用回读就能排除的错误。当前没有足够证据认定哪一个运行时环节出错。
- 新增 `Tools/Unreal/VFX/FallingFoliage/AuthorFoliageShaderMeshControlUE58.py`，拟建第二道 StaticMesh 四材质对照关卡，鉴别材质本身与Niagara路径。其Python语法已通过，但第一次UE编辑器运行180秒后超时（UE模块加载耗时长，且有共享UBT互斥锁），**尚未创建真实关卡、尚无GPU截图，不宣称成功。**
- 仍待完成：StaticMesh对照的UE作者执行和真实D3D12截图、最终根因修复、平台运动模块经备份有门禁的修改、高空飘落/地面风吹/角色选择与创建场景复测、整体验收与GPU性能量化。本轮未触及正式平台母材质及 Niagara 运动模块。**总状态：未完成。**

