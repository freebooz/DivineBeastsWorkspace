# SSLM全项目过期脚本清理（2026-10-31）

用户先限定资源/鸟群/地形，随后明确扩展到整个项目。本轮通过本地工具盘点、哈希及调用关系检查，采用可恢复归档；未调用HWebCodex，不操作Git索引、不清原生资产或工程外目录。

## 盘点与处置

审查时898个受维护脚本：691 Python、191 PowerShell、15 PSM1、1 CMD；Tools546、Build54、Tests272、Deploy6、Game19、Docs1。4697个维护区文本文件参与引用扫描，Git受管与未跟踪脚本集合复核无差异。排除生成目录、Saved、SourceArt、第三方原版与既有Archive；不把普通产品源码当过期命令文件清理。

238项未发现直接文件名/模块引用，但可能由测试发现、人工或外部工具执行，不能据此退休。两个重复哈希组中，四项受SpellPresentation合同保护的退休兼容stub保留；另一组是完全相同的Palm采集Review/Probe，只保留Review为当前入口。

| 原入口 | 处置与依据 |
| --- | --- |
| Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockAssets.py | 首版旧网格写入，当前Flight已有唯一作者；归档Resources/20261031 |
| Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockAssets.py | 只校验首版_Cm，当前P4校验器替代；同上 |
| Tools/Unreal/Terrain/ResumeTerrainV3FormalCover.ps1 | 只适用9/10与Dirt待办，实际10/10已完成；同上 |
| Tools/Unreal/PCG/PromoteVillageForestReview.py | 已完成的冻结SHA一次性森林升正式；归档Project/20261031 |
| Tools/Unreal/VFX/DevelopmentMagic/CapturePalmRedesignProbeUE58.py | 与保留的Review字节完全相同；同上 |
| Tools/Unreal/VFX/DevelopmentMagic/ProbeFireballEditorAutoExec.py | 八tick执行探针已完成，无当前调用；同上 |
| Tools/Unreal/PluginDemos/BindManualPCGRealizedMonolith.py | 冻结收据绑定已完成，Profile SHA及独立冷读吻合；同上 |

七项原字节共45548字节，均保留在工程Saved备份；归档文件在执行/导入前拒绝，防止重新发布退休身份、重复绑定或关闭当前Editor。原路径、精确归档路径、原始及归档SHA的唯一清单为`Tools/Archive/ScriptRetirementRegistry_20261031.json`。没有当前脚本、构建或测试继续调用已移走的入口；当前索引已更新，旧报告/来源清单保留当时事实。

## 证据与验证

证据根`Saved/Validation/ResourceOwnershipRemediation20261031/ScriptAudit`：ProjectScriptReferenceInventory.json、ProjectScriptExpiryCandidates.json及ArchiveOriginals。归档前核对文件未漂移、当前进程无直接候选命令行引用、重复字节及完成收据；原始备份可通过登记SHA独立复核。外部任务计划、手工命令及UE二进制内脚本名未穷尽，故不宣称发现全项目所有死代码。

最终894维护入口保留，原库存减少的4项均有精确归档记录；此前先归档的3项另计。693份Python与207份PowerShell（含7项归档）语法解析0错误；七项原始字节和归档SHA核验通过，六份Python归档在导入Unreal或注册回调前拒绝执行。资源合同/退休作者回归10/10通过。脚本归档不作为UE功能、GPU或人工验收证据；本轮原生资源迁移、完整构建与Cook分别见`Resources/ResourceOwnershipDelivery_20261031.md`。

既存独立问题：ValidateWorldManualFixturesMonolith.py仍要求旧四节点四边，而已实现的Realized Profile为九节点九边；保留并登记，本轮不修改该验证基线。
