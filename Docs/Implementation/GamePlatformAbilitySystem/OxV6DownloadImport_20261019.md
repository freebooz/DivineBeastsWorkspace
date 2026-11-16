# 丑牛 V6 下载包接收与原生资源核对

2026-10-19按用户要求接收下载目录 `SSLM_OxV6_8Review64_8Runtime48.zip`。ZIP SHA-256：`0a7554120192501ab58648c719031a747a3e34b7e9a029276a8c08997bcff743`。来源清单原件与逐文件结果保存在 `Saved/Validation/HeroSkillIconWorkflow20261019/OxDownloadOriginalManifest.json` 和 `OxDownloadReceipt.json`。

## 实际接收结果

包内8张Review64、8张Runtime48的真实尺寸与SHA全部通过，16张PNG均已在 `DBAHeroPack_Ox/SourceArt/UI/SkillIcons/V6/{Review64,Runtime48}`，逐字节与ZIP相同。8张既有 `/DBAHeroPack_Ox/UI/Abilities/V6/Textures/T_DBA_Ox_V6_*_Icon` 原生Texture2D均为48×48、sRGB及无损UI压缩，其真实导入源SHA分别与下载清单匹配。

因此采用幂等接收并复用现有8张资源，没有重写相同PNG/uasset。原始ZIP保留，未执行压缩包内代码。包自述 `DERIVED_FROM_EXISTING_PROJECT_OX_ART`：本图仍是已有美术衍生审核版，`PENDING_HUMAN_ART_REVIEW` 不变，不能将它报告为另一套独立绘制的终稿。

## Profile、ViewModel与小尺寸

已核对现有唯一 `DA_DBA_Ox_PlaceholderUIProfile`：原六占位条目及八项V6表现条目，共14项，稳定AbilityId和图标路径正确。沿用既有事件驱动技能栏ViewModel和数据租约链；没有GAS授权时保持空槽，不产生伪施法按钮。

本次四英雄统一小尺寸检查包含牛包16图，尺寸/SHA/字节去重通过；「不动玄门/立阵」48图dHash距离1，仍需人工复核。技术证据为 `SmallSizeAudit.json` 和 `Ox_Source64_48.png`。只读审查蓝图 `/DBAHeroPack_Ox/UI/Review/WBP_DBA_Ox_SkillIconReviewV6` 已由Monolith真实创建、编译和保存，含64/48逻辑像素两行Image，明确标注“仅美术预览、不可施放”。

HUD原生帧验证、清理及最终结果以同证据目录的 `Ox_HUDPreview.json` 与实际PNG为准；文件请求或蓝图保存本身不算截图完成。完整方法见 [导入后接线流程](HeroSkillIconImportWorkflow.md)。联机准入、Cook和人工美术批准分别记录，不由本次接收包自动通过。
