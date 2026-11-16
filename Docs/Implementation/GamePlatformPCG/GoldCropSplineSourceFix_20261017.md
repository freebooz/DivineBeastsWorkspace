# SSLM PCG 金标准农田零实例修复记录（2026-10-17）

## 一、问题与真实证据

SSLM唯一正式工程为 `Game/DivineBeastsArena.uproject`，引擎锁定UE5.8。金标准关卡 `/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1` 的实时预览最初在冠层图出现0实例。

既有会话已对三份Gold Realized Graph（实际生成图）加入官方DensityFilter（密度过滤）并在独立UE进程确认。随后真实预览日志 `Saved/Validation/GamePlatformPCG/SpatialProbes/c3c06082-2b5c-4920-8842-ca866b7ba2fc/UE_Editor_GeneratePreview.stdout.log` 显示：
- `PCG_Canopy`（林冠）实际5个有效实例，非碰撞/导航/网络复制检查通过。
- `PCG_Rock`（岩石）实际22个有效实例，排除带和实例审查通过。
- `PCG_Crops`（农作物）仍为0实例，原生诊断 `TaggedData=0 Points=0 Managed=0`。不能据此宣称G01～G16全部通过。

## 二、UE5.8源码级根因

平台 `AGamePlatformPCGPolygonActor` 包含 `GenerationBounds`（范围盒）和 `Boundary`（闭合样条）。UE5.8的 `UPCGComponent::CreateActorPCGData` 默认解析Actor组件，多个空间组件可被组合为 `UPCGUnionData`。

引擎 `PCGSplineSampler.cpp` 内的 `PCGSplineSamplerHelpers::GetPolyLineData` 仅从 `UPCGPolyLineData`、`UPCGSplineProjectionData` 或 `UPCGIntersectionData` 提取可采样样条；它不将复合的 `UPCGUnionData` 自动解为样条。此前 `TPL_CropField`（作物地块模板）将图默认输入直接连接到 `SplineSampler.Spline`，因此在混合输入情况下出现0输出点。

不能通过降低 `TotalInstances > 0` 门禁、伪造点或直接让服务器运行客户端PCG解决。

## 三、通用层已实施的源代码修改

- `GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp`：为道路、围合、闭合围合、地块填充与农田模板新增 `UPCGDataFromActorSettings`（官方获取Actor数据节点），限定 `ActorFilter=Self`（仅当前Actor）、`ComponentSelection=ByClass`、`ComponentSelectionClass=USplineComponent`。实际连线由 `GetActorData.Out → SplineSampler.Spline`，不再读取复合图默认输入。
- `GamePlatformPCG/Private/Validation/GamePlatformPCGTemplateContract.cpp`：登记官方节点类型允许列表，不放宽其他任意PCG设置。
- `GamePlatformPCG/Private/Validation/PCGGraphInspection.cpp`：对GetActorData参数增加白名单，拒绝遍历其他Actor、子Actor、任意组件、动态重查询，以及未连接样条输入的已实例化CropField图。
- `GamePlatformPCGEditor/Private/Tests/PCGEditorTests.cpp`：检查真实 `UPCGEdge` 从受限来源连接至样条采样器，并检查全部五种线性/地块模板的来源约束。

这些都是源码层变更，新增图模板不代表旧已保存资产已经更新。

## 四、旧Gold Crop图原生迁移入口

旧 `PCG_Gold_Crop.uasset` 当前仍需由UE5.8真实编辑器创建受限GetActorData节点并重连图。本轮建立两个工具：
- `Tools/Unreal/PCG/RepairGoldCropSplineInputUE58.py`：仅访问Gold Crop开发图，分Inspect（只读检查）、Apply（迁移）和Verify（独立冷加载核验）。
- `Tools/Unreal/PCG/RunGoldCropSplineRepairUE58.ps1`：验证引擎版本与独占编辑器/构建状态；Apply前SHA-256备份，独立引擎执行后核验日志成功标记、文件变更/未变更约束，避免Python异常被零退出码掩盖。

正式Village与MainArena地图、项目内容包和专服碰撞不在修改范围。编辑器存在其他活跃用户进程时工具必须失败关闭；不会终止其他任务、覆盖其蓝图，不能跳过引擎原生资产修改而声称迁移完成。

## 五、验收顺序和通过标准

1. 静态脚本语法、现有PCG架构门禁及原生C++语法/目标构建检查。
2. 无编辑器并发时执行Gold Crop `Inspect → Apply → Verify`，确认真实图唯一受限GetActorData节点、正确Spline连线以及没有其他拓扑意外改变。
3. 正式编译最新PCG Runtime、Editor模块，并执行 `GamePlatform.PCG.Editor.FoundationTemplateContracts`、`RealizedGraphMeshSet` 和基础三层门禁。
4. 独立运行 `RunGoldSpatialProbe.ps1 -Stage GeneratePreview`，要求Canopy/Rock/Crop三组输出有效实例，空间排除与非碰撞合法，成功标记存在；否则依然失败。
5. 分别执行独立进程确定性G09、金标准G01～G16，之后才能推进Village正式资源、烘焙与Client/Server双Cook。

开发测试网格不能作为已发布的3A正式资源。尚无开放世界正式内容包、正式Gaea原始地形或第三方许可合规的完整植被资产，不得将Gold样例验收等同于全游戏交付。

## 六、2026-10-17 本次续接实测与剩余阻断

- 读取15:43（UTC+08）前保存的Gold生成日志：经DensityFilter修复，正式UE5.8在独立进程成功生成Canopy 5实例、Rock 22实例，Crop图仍为0点/0实例。这是修复前剩余失败的明确复现。
- 新增通用样条来源读取后，使用锁定UE5.8编译参数、MSVC 14.44与`/Zs`（仅C++语法验证）检查`PCGGraphInspection.cpp`、`PCGDevelopmentGraph.cpp`、`GamePlatformPCGTemplateContract.cpp`和`PCGEditorTests.cpp`，4个编译单元均返回0。此检查不链接新的DLL，更不代表正式UE Editor目标构建完成。
- 通过`Tests/Architecture/VerifyPCGGoldAuthoring.py`、`ValidateVillageTropicalMountainForest.py`、`ValidateVillagePCGTechnicalMeshes.py`、`VerifyPCGArchitecture.ps1`及`TestPCGGameCoverage.ps1`等现有静态门禁；PowerShell AST及Python AST检查新增迁移工具通过。
- 当前仍有他人打开的`UnrealEditor.exe`，安全执行`RunGoldCropSplineRepairUE58.ps1 -Mode Inspect`时被排他门禁拒绝（退出1），修复前后Gold Crop文件SHA-256完全一致。该拒绝是预期安全行为，不是农作物真实迁移或验证通过。
- 既有真正UE地图盘点显示Village已加载10个Actor、1个Landscape，但已加载PCG放置器/Director均为0；MainArena已加载23个Actor，PCG放置器/Director均为0。证据为`Saved/Validation/GamePlatformPCG/WorldAudit/`内两份`PCG_*_NativeWorld.json`。它只统计当前加载Actor，不能代表WorldPartition未加载单元。
- 生产状态：**金标准Crop来源链的C++和受控迁移工具已准备，既有.uasset迁移未执行，最新C++未经正式UE编译链接，Gold GeneratePreview/G09/G01～G16仍未复验，Village正式地图PCG仍未落地。**不能标记为生产完成。

## 七、18:04 后续接检查与统一恢复入口

- 当前唯一工作空间的正式UnrealEditor.exe在17:27启动，且18:04后仍活跃。没有取得独占UE Editor/UBT权限，故禁止覆盖Gold Crop、重新链接当前已加载的PCG DLL或执行正式地图保存。
- RunGoldCropSplineRepairUE58.ps1（Gold Crop单图迁移入口）增加了5项只读保护对象的SHA-256前后对照：Gold Canopy/Rock图、GoldLevel地图、Village地图与MainArena地图；并记录独立result.json，包括目标图修改前后指纹、UE进程退出码和本次日志标记。该入口不会以资产存在、进程退出0替代真实PCG实例生成。
- Tests/Architecture/VerifyPCGGoldAuthoring.py（金标准静态合同）新增Crop受限Actor样条来源、查询安全白名单、原生负向用例、备份和幂等准入的静态回归；结果输出PCG_GOLD_CROP_SPLINE_STATIC_CONTRACT_PASS。
- 新增Tools/Unreal/PCG/RunGoldCropRecoveryUE58.ps1（统一续接工具）。不带-Apply默认为只读Plan；显式-Apply时按Gold Crop Inspect、Apply、Verify、PCG Runtime/Editor增量编译、8项UE原生Automation、三类Gold图真实GeneratePreview和G09两独立进程指纹比较的顺序执行。任何阶段失败均停止，在Saved/Validation/GamePlatformPCG/GoldCropRecovery下保存阶段日志和result.json；ProductionReady始终为false。
- 实测Plan返回0且状态BlockedByActiveEditorOrUBT；显式Apply被当前Editor占用门禁拒绝，返回2，农作物图SHA-256未变。Plan证据位于GoldCropRecovery/8b411abd7f294979b75f44b565129152，Apply拒绝证据位于GoldCropRecovery/8d035a8a84774132961721bcdebecadb。
- 金标准静态回归、PowerShell脚本AST及git diff --check均退出0。**当前没有最新的Gold Crop原生迁移完成、C++模块正式链接、UE自动化、生成预览或G09通过证据，不能宣称这些阶段已执行。**
