# SSLM 工程命名审计与修正记录（2026-10-28）

## 范围与标准

以 `AGENTS.md`、`Game/Plugins/插件开发规范.md` 和 `Tests/Architecture/TestEnglishProjectNames.ps1` 为现行规范。技术身份 `DivineBeastsWorkspace` / `DivineBeastsArena` / `DBA` / `GamePlatform` 保持不变，`SSLM` 仅作为项目展示代号。项目目录和非文档文件名使用英文 ASCII，文档名可用中文，插件 FriendlyName 为英文。UE `.uasset/.umap` 若需要重命名，必须经过引擎原生 Rename、引用/重定向审计，不可直接更改磁盘文件名。

初次扫描 `TestEnglishProjectNames.ps1` 检查 **636,030** 项路径和 **70** 个活动插件描述，发现 **30** 项违规。对 11,290 条 Git 已跟踪路径额外检查：未发现路径大小写冲突，未发现带空格的已跟踪 UE 资产文件名。本次不把静态名称检查当成 UE Shader、Client/Server Cook 或运行时通过。

## 实际复核结果

- 修正后重新运行 `TestEnglishProjectNames.ps1`：检查 **636,058** 项路径、**70** 个活动插件描述；违规数由 **30** 降至 **29**。剩余 **29/29** 全部位于 `Saved/Validation`，活动源代码/源美术及正式资源目录没有非 ASCII 路径违规；全工作空间门禁仍返回退出码 1，不能声明全面通过。
- 额外检查 `Game/Plugins`：**70** 份 `.uplugin`，**91** 个声明模块、**20** 个纯内容插件；插件文件名与目录、模块唯一性及 `Source/<Module>/<Module>.Build.cs` 一致性检查 **0** 异常。
- 参考 PNG 的 SHA-256 与更名前完全一致；三个 Python 脚本已用 UE5.8 随附 Python 解释器完成 AST 语法解析，`EnergyTextureManifest.json` 通过 JSON 解析和路径/哈希验证；`git diff --check` 未报告文本空白错误。
- 审查覆盖可脚本化的文件/目录、插件、模块和已知工程资产身份。**未对全部 C++、Go、蓝图和第三方 `.uasset` 的内部语义名称逐项人工审查**；这些不因静态文件名检查而自动算作完整通过，也不在当前任务中进行大规模破坏性重命名。

## 本次实际修复

- 将 `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash/Reference_凝锋.png` 更名为 `Reference_DBA_AttackSlash_CondensedBlade.png`。
- 原 PNG SHA-256 为 `A9E0E6B006EA1EA55E73BCCF19F677E0D61418E967E69952C22B22D038D907AB`，新文件必须保留相同字节和哈希。
- 同步修改 `BuildAttackSlashV3Blend.py`、`CaptureAttackSlashV3UE.py`、`PublishAttackSlashV3Evidence.py` 的参考图引用及 `EnergyTextureManifest.json` 的 `referenceFile`；不改清单 `referenceSHA256`。
- 该文件是 Blender/Niagara **原始参考 PNG**，不是引擎资产，因此使用 Git 跟踪的文件移动，未生成或伪造 UE Redirector。既有历史 `AttackSlashV3.blend` 如包含旧参考名属历史内部元数据，不直接写二进制修改。

## 不直接改名的项目

- 剩余 **29** 项位于 `Saved/Validation`，属于已存在的 VFX 历史截帧和一处有意用于 Unicode 路径兼容性测试的夹具。重命名将破坏外部报告和历史证据引用；本次保留原文件，仍据实列为全仓库路径检查未通过。
- `Game/Content/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1.umap` 未使用常见的 `L_` 关卡前缀，但已有 C++、Python、PowerShell、资产清单和演示内容引用，是稳定 UE 包名；现行 ASCII 规则并未直接禁止 `PCG_` 命名。未经 UE 原生迁移和实际运行回归不得强行改为 `L_`。
- `Game/Content/PN_OpenWorldFoliage` 与 `Game/Content/StyleHex_Studio` 为外部导入的 Fab 供应商资产，保留原始资产名、材质引用和许可边界，项目层派生内容新建时按 DBA 身份命名。
- 并行进行的 Fab/LandscapeGrass 与村庄地形资产属于其他未完成操作，未接管、不覆盖、不批量更名，也不修改正在运行的编辑器状态。

## 复核与回退

变更完成后检查 PNG 文件 SHA-256、四处引用、JSON 可解析、Git 记录的重命名结果和只读命名检查；如果历史 Saved 路径仍被报告，诚实保留，不以删除证据或修改测试规则制造零违规。源图重命名可通过 `git mv` 逆向恢复，但应同时恢复脚本与清单路径，避免悬挂引用。