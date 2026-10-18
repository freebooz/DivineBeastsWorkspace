# SSLM 地形材质与平台材质函数真实集成（2026-10-17）

工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`，引擎：锁定 Unreal Engine 5.8（虚幻引擎）。

**2026-10-18颜色规范增量（保留本文件历史函数实现结论）**：V3.0参考色见[唯一全局色板](../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)，实际扫描十层整改/UE原生V3候选与**失败的黑帧GPU审核门禁**见[专项目录及实物记录](TerrainColorV3ComplianceReview_20261018.md)。本文件保持既有八处MF调用及RVT未验结论，不能用新基础色资产存在替代正式地形或人工签收。

## 一、实际缺口及改造范围

原正式新手村 `M_DBA_Village_LandscapeBridge`（客户端/服务器共用中性地形）、旧扫描母材质 `M_DBA_Terrain_Production10`（十层高精地形）只有独立节点，没有真实使用平台 `MF_GP_*`（通用材质函数）。先采用独立候选的可回退改造，而不冒险将客户端平台资源硬引用到服务器地图。

所有新增原生资产均归已有三层的正确所有者，不新建插件：

- `/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PBRLayerBlend`（通用平台四通道PBR覆盖函数）：9个输入，4个输出；Color/Normal/Roughness/AO（基础色、切线空间法线、粗糙度、环境遮蔽）共同按同一覆盖遮罩混合并归一化法线。
- `/DBAPresentationPack_Core/Materials/Terrain/M_DBA_Terrain_Production10_FunctionIntegrated`（SSLM项目纯客户端函数化十层母材质）：继续使用原有10个 `LS_*`（地形图层）和40张Poly Haven CC0 PBR贴图，保持原材质及原地图资源身份。
- `/DBAPresentationPack_Core/Materials/Terrain/MI_DBA_Terrain_Production10_FunctionIntegrated_Review`（高精候选审核实例）：继承上述新母材质。
- `/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview`（隔离的十层地形审核地图）：已经通过UE5.8“独立进程复制原审核地图→另一个进程绑定函数化MI→第三个进程冷重载”三段验证；只用于开发评审，不得作为正式新手村地图。

## 二、真实函数接入与设计取舍

候选地形包含八处有真实参数与输出连接的 `UMaterialExpressionMaterialFunctionCall`（材质函数调用）：

1. `MF_GP_LandscapeMacroColor`（宏观颜色）：一处，轻量宏观色变化，仍保留原PBR色调。
2. `MF_GP_LandscapeDistanceDetail`（距离细节）：一处，近景原扫描颜色向远景小幅调整，避免过度发白。
3. `MF_GP_LandscapeBiomeMask`（地貌/生态遮罩）：一处，约束Rock与ForestSoil（岩石/林地土壤）的苔藓覆盖。
4. `MF_GP_LandscapeSlopeLayers`（坡度层）：一处，计算面朝上积雪遮罩；使用顶点法线，不让材质输出Normal反向参与遮罩。
5. `MF_GP_LandscapeAltitudeLayers`（海拔层）：一处，以厘米为世界高度单位。
6. `MF_GP_PBRLayerBlend`（PBR四通道混合）：三处，分别用于苔藓、湿润、积雪，保留真实纹理颜色/法线/ORM通道，不采用既有旧版简单苔藓/积雪函数里的固定替代颜色。

`MF_GP_HeightBlendMask`（通用双高度遮罩）当前公式在图层权重为0时仍可能产生非零遮罩；为避免引入未经绘制的地表层，**十层原有微高度权重算法暂时保留**，不强行为凑调用数量替换。若后续升级其平台合同，应添加严格的输入层权重零值门禁与Shader回归。

`MF_GP_LandscapeRVTAdapter`（RVT适配）资源存在，但工程尚无可验收的地形RVT真实写入/采样资源与运行证据，**未接成假调用**。正式Village共享母材质亦未修改，必须先通过客户端高精装配和Server Cook隔离验证才迁移。

## 三、源代码和执行工具

- `Tools/Unreal/Surface/AuthorTerrainPBRLayerBlendUE58.py`：平台MF真实UE作者；默认inspect（只读），apply（新建）、verify（原生回读）均拒绝错误引脚合同。
- `Tools/Unreal/Terrain/AuthorTerrainProductionMaterial.py`：默认模式不改旧母材质；仅 `TERRAIN_FUNCTION_INTEGRATION=1` 明确指定函数化新候选。执行时对平台MF做真实反射引脚预检，再构造、编译、保存并回读调用节点。
- `Tools/Unreal/Terrain/RunTerrainFunctionCompositionUE58.py`：同一编辑器进程顺序生成平台MF和候选母材质，支持inspect/apply/verify。核验须在第二个独立进程进行。
- `Tools/Unreal/Terrain/AuthorTerrainProductionQualityProfiles.py`：三档High/Medium/Low（高/中/低）MI作者；本次已创建三份独立候选MI并在UE5.8冷重载回读。它们仅配置颜色、平铺、距离和苔藓等有效参数，不承诺实际GPU提速。由于当前UE5.8静态开关写入接口返回False，函数化候选三档全部继承母材质的HeightBlend=false（关闭高度混合），不宣称已实现高度混合的高中低切换。
- `Tools/Unreal/Terrain/AuthorTerrainFunctionReviewMapUE58.py`：先在独立编辑器进程使用`TERRAIN_FUNCTION_REVIEW_MAP_MODE=copy`复制已有十层审核地图，结束进程后第二进程以`apply`绑定函数化MI，再以第三进程`verify`冷重载。禁止一次进程中复制并立即LoadLevel（UE5.8 UWorld未被垃圾回收时会触发Fatal）。
- `Tools/Unreal/Terrain/RunTerrainFunctionReviewUE58.py`：独立编辑器回读与三档MI制作入口；其`apply`必须以已经独立复制好的审核地图为前置条件，随后对审核地图进行幂等材质绑定；使用UE/UBT独占窗口，不能并行抢占编辑器。
- `Tests/Architecture/TestTerrainMaterialFunctionIntegration.py`：不依赖UE的函数引脚、原旧材质身份、三层方向、地图隔离静态检查。

真实使用时在独占的 UE5.8 编辑器命令行环境指定变量，例（PowerShell）：

```powershell
# 正式PBR/Pipeline原版不会被默认命令覆盖；仅显式创建独立候选。
$env:TERRAIN_FUNCTION_COMPOSITION_MODE = 'apply'
& 'D:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'Game/DivineBeastsArena.uproject' '-ExecutePythonScript=Tools/Unreal/Terrain/RunTerrainFunctionCompositionUE58.py' '-unattended' '-NoCompile' '-DisablePlugins=Monolith'
# 第二个进程使用verify，实际UE重新加载8个函数调用及四通道MF合同。
$env:TERRAIN_FUNCTION_COMPOSITION_MODE = 'verify'
# 之后仅在需要额外审查地图和质量MI时，才显式执行RunTerrainFunctionReviewUE58.py。
```

## 四、本轮真实执行记录与未完成门禁

- 2026-10-17：默认原作者与候选作者执行Python语法、DryRun（只读计划）及 `TestTerrainMaterialFunctionIntegration.py` 新增静态检查，全部通过。已有 `ValidateTerrainProductionIntegration.py`（原十层源图/三层静态门禁）继续通过。
- 通过本机 `D:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor-Cmd.exe` 运行实际 **apply**：日志 `Game/Saved/Validation/TerrainFunctionComposition_20261017/AuthorApply.log` 确认平台函数9输入/4输出/20原生表达式生成、候选UMaterial 291表达式与8个函数调用、候选MI保存，`TERRAIN_FUNCTION_COMPOSITION_UE_VERIFIED mode=apply`，进程退出码0。日志未报告Shader错误；仍须独立进程反射及实际GPU截图验证。
- 首次质量档`apply`因UE5.8新建MI无法对`TerrainEnableHeightBlend`（高度混合静态开关）写入而失败；刷新参数缓存后仍失败；随后确认关闭分支时`TerrainHeightBlendStrength`无法写入。仅对独立函数化候选采用保守修复：高/中/低档统一继承母材质HeightBlend=false、剔除关闭分支无效参数，标量写入失败只有在母材质确有相应参数且回读值等于预期时才允许继承默认。日志`Game/Saved/Validation/TerrainFunctionComposition_20261017/QualityApplyScalarSafe.log`真实退出0，三档MI原生保存。
- 审核地图首次尝试在同一UE进程`DuplicateAsset`后直接`LoadLevel`导致UE5.8 `World Memory Leaks`（世界对象无法释放）Fatal，退出码3；修复为三个独立Editor进程：`MapCopy.log`（复制保存退出0）、`MapBind.log`（唯一Landscape绑定MI并保存退出0）、`MapColdVerify.log`（独立冷重载通过，退出0）。原审核地图与正式Village地图均保持原引用。
- 最新独立UE5.8冷重载`Game/Saved/Validation/TerrainFunctionComposition_20261017/FunctionColdVerify.log`，进程退出0，包含`TERRAIN_FUNCTION_REVIEW_NATIVE_VERIFIED mode=verify`、291母材质表达式/8函数调用、3个真实MI回读、1个Landscape绑定候选审核MI。另以`Tests/Assets/ValidateTerrainFunctionAssetReferences.py`检查原生资产序列化引用，防止候选误进入服务器共享地图。
- PCG真实实例生成/烘焙、D3D12六态GPU画面、高中低性能预算、Client/Server新版本Cook/Stage/Pak、Gaea 2.3正式高度及Mask源与最终人工美术签收仍未执行，不得将本轮原生资产制作视为发布完成。
- `Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`（总体目录规划）当前已存在多处 `<<<<<<<`、`=======` 和 `>>>>>>>` 未解决合并冲突标记，属于预存文档问题。本轮不擅自解决与地形无关的历史冲突；本说明与Terrain/Surface README作为新增资产和职责的补充索引。

## 五、后续必须满足的生产准入

独立编辑器重新加载 → 8个函数调用实际引脚及Shader通过 → 六态天气GPU画面（晴、苔藓、湿润、轻雪、重雪、融雪）→ High/Medium/Low帧耗真实测量 → 客户端包含高精资产、Server包不含纯表现资产的Cook/Pak审计 → 人工批准。没有正式Gaea源不得认定地貌数据正式发布；没有这些证据不得以新的候选母材质覆盖 `L_Village_Start`。
