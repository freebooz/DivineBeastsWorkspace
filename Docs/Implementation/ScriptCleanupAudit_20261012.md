# 《SSLM》脚本与命令文件安全清理审查（2026-10-12）

工程：`DivineBeastsWorkspace`，执行插件：HWebCodex。本文记录执行计划、证据、保留与删除原因。历史恢复依赖 Git，不执行 `git clean`、`reset`，不修改 UE 资产。

## 1. 审查范围与静态证据

- 初始 HEAD `ecb9d85617947b83996dafd8914fab15e8c252e2`、分支 `main`；开始时有约 53 项其他工作中的未提交修改，本次不修改、不暂存或提交这些内容。
- 已盘点受 Git 版本管理的 331 个脚本：222 个 `.py`、100 个 `.ps1`、9 个 `.psm1`。受版本管理范围内未发现 `.bat/.cmd/.sh`。不将 UE 引擎生成目录中的临时命令文件作为人工维护入口。
- 扫描 3239 个仓库文本文件（构建、插件、源码、测试、部署、配置、文档），补充扫描 24 个未跟踪文件；记录每个脚本完整文件名被引用情况、调用文件类别及文件体积。80 个未发现直接文件名引用；未发现内容字节完全相同的脚本。报告存放 `Saved/Validation/ScriptCleanup/ScriptReferenceInventory.json`。
- **没有引用不代表闲置**：测试可能通过目录发现，手动 UE Python／PowerShell 入口可能由开发者直接调用，外部自动化与二进制资产内引用也未完全覆盖。未能明确排除这些用途的脚本全部保留。

## 2. 分阶段执行计划

- [x] P0：读取 `AGENTS.md` 与插件规范、版本状态、候选调用图，保护其他任务的修改。
- [x] P1：静态扫描、重复文件扫描、Git 历史与脚本内容逐项检查。非高置信度候选不得删除。
- [x] P2：选定确认为一次性探针或已废弃破坏性旧入口的 4 个脚本，核验当前没有活跃进程使用，候选文件没有未提交修改。
- [x] P3：确认四项均已删除；本会话实际执行2项（旧前端地图原型生成器、PCG反射探针），另外2项（Python执行探针、Niagara API探针）由并行清理会话独立删除，本会话未重复操作。并行会话另删除3项旧鸟群脚本，记录为其他会话范围。
- [x] P4：剩余受管Python 215项静态AST解析0错误，PowerShell 109项语法解析0错误；18项鸟群发布策略测试全部通过，PCG正式校验入口的inspect模式退出0，英文命名门禁122501项／64插件／0违规，正式前端及角色工作室地图SHA-256均与清理前一致。未因此重编译UE或运行真实地图，不能冒称其通过。
- [x] P5：总体目录规划原有并行清理章节已作唯一锚点增量更新，采用原始字节哈希对比和独占锁以保留既存的混合换行及其他任务修改；更新前备份仅存Saved/Validation/ScriptCleanup。本报告列出责任边界和恢复方式。

## 3. 确认清理的四项

1. `Tools/Unreal/Characters/TestExecutePython.py`：旧一次性 Python 执行探针，硬编码个人磁盘路径写入 `Game/Saved/PythonExecuteProbe.txt`；无调用／标识引用，正式 UE 执行与验证链路已建立。Git blob `2415fcbfa634e546fcbc2a6218c8c7c3db2a59d8`。
2. `Tools/Unreal/FrontEnd/GenerateFrontEndMaps.py`：旧地图原型生成入口，无条件先删除正式登录地图与角色工作室再重建，与现有无损修改／备份方案冲突，误调用会破坏用户当前关卡。现有 `UpdateCharacterPreviewStudio.py` 和 `ImportZodiacSanctuary.py` 是受控流程；无引用。Git blob `416038d037d3e1854d5f050ad15664801ec76177`。
3. `Tools/Unreal/VFX/BirdFlock/InspectNiagaraEditorAPI.py`：UE5.8 Niagara 编辑器一次性 API 能力探针，仅打印属性，不产出或验证正式鸟群；已有实际 Niagara 资产作者与正式资源验证器。无引用。Git blob `9fd1b2865d43cc62bd34510df2afb1adcb001e57`。
4. `Tools/Unreal/PCG/InspectGoldReflection.py`：PCG 初始反射探索脚本，仅打印字段，正式 `ValidateGoldLevelAssets.py` 已承接真实资源回读与定义检查。无引用。Git blob `a62bfd52273c75b89a169c6636a05b6147f8611b`。

这四项均无对应活动调用。本会话直接删除的两项在删除前Git干净，另两项于删除请求前已由并行会话处置，不归入本会话执行数。Git 保留全部历史；不删资源、角色预览地图或UE编辑器文件。

## 4. 保留范围与疑似冗余

首轮80个未发现直接文件名引用的脚本中有6项由双方审查后处置（本会话2项、并行会话4项，另有1项并行删除脚本仅有文档引用），因此其余 **74 个没有直接文件名引用的脚本仍保留**。包括 `Tests/Integration/Online` 测试、地形／落叶／天气／PCG／鸟群／Fauna 资源生成器和审核工具。特别保留可重建资产的 `AuthorBirdFlockAssets.py`、真实GPU校验、CMake构建入口及自动化测试；没有直接引用不能据此认定废弃。另一会话使用更宽口径统计360项（额外含CMake和未跟踪脚本），两份统计口径不同，并不矛盾。

## 5. 恢复与风险

删除后使用 `git show ecb9d85617947b83996dafd8914fab15e8c252e2:<path>` 可只读查看历史；如有未登记的外部调用者，需要先核实需求和安全性，再从版本库恢复。旧 `GenerateFrontEndMaps.py` 不建议直接恢复执行，否则可能删除当前制作中的正式场景。

外部个人脚本／任务计划调用、手工命令历史、UE 二进制内字符串不能在本轮静态扫描中完全排除；因此结论仅覆盖四个证据充分的候选，不承诺项目所有潜在死代码已被一次性找完。

## 6. 验证结果

本会话实际通过 HWebCodex 结构化文件工具删除2个Git受管脚本；另5个是并行审查会话独立删除，不能计入本会话操作。本轮没有删除任何 .uasset／.umap、源美术或构建／部署入口，也没有触碰其他未提交修改。Git 工作区对这7个脚本均显示 `D`，全部可从原HEAD逐项恢复。

验证已实际执行并核对退出码：Python剩余215个AST解析0错；PowerShell109个解析0错；鸟群容器发布策略18/18通过；PCGGold正式校验脚本inspect模式退出0（仅配置核对，非UE启动）；英文命名检查122501个条目、64插件、0违规。没有执行UE全编译、真实Cook或实际图形验收；因为仅删除已经无调用的一次性工具，这些高成本工作不在此次校验声明范围。执行时另外3项旧鸟群脚本的删除由并行会话控制，须以其报告作为独立证据。

删除前后正式前端地图 SHA-256 保持一致：
- `L_DBA_FrontEnd.umap`：`DA86BC7D89DD0356795715ADDD5BB3DEE2B82F9580B0AAE823EAAC22C6598371`
- `L_DBA_CharacterStudio.umap`：`95F8175314FD70C8A96DB15FC37C6AE04695D5D5440A85967E025FCEAAC7A8D4`
