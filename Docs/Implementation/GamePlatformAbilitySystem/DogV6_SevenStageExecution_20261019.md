# SSLM 戌狗 V6 七阶段执行记录（2026-10-19）

本轮依据用户的七项明确指令冻结戌狗 V6 八技能美术表现定义；不更改历史V6 JSON的待审批状态、不改动V5生产正式名称和GAS授权。具体八技能、AbilityId、特殊Slot4/Slot3顺序及视觉主体见`Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog/Docs/DogV6_ArtFreeze_20261019.json`。

阶段状态：
1. **冻结设计**：已经从V6及V5数据核对，并将八技能冻结为独立美术合同。
2. **精修图标**：ChatGPT工作目录中暂存真实8张48像素、8张64像素及8张1024插值参考图，AI海报裁切源实际有效分辨率约96～183像素，不是独立高清原生母版；人工艺术验收待完成。
3. **导入英雄内容包**：未完成。HWebCodex导入附件时收到`host-provided download URL resolves to a non-public address`，目标Windows工程没有V6源图；原生Texture2D不得伪造。
4. **唯一UI Profile**：拟在真实纹理到位后原位增补`DA_DBA_Dog_PlaceholderUIProfile`并保留旧条目及二进制备份；正式生产名称继续使用V5，V6名称可作为开发美术审核名称。
5. **技能栏ViewModel**：复用现有`UDivineBeastsAbilityBarViewModel`的`ArtPreviewAbilityIds`和只读预览开关，禁止图标授权可施放GAS技能；无真实纹理前不伪造运行绑定。
6. **64/48验收**：本地暂存PNG完成文件尺寸及SHA-256检查；真实UE导入源色、UMG布局、DPI与人工辨识度验收未进行。
7. **新手村与战斗HUD**：未进行。必须在真实UE5.8和正常客户端/服务器版本校验后取得截图和日志。当前未证明Monolith本会话可调用，也没有正在运行的正式UnrealEditor进程。

## 编辑器工具和真实可调用入口

- `Tools/Unreal/UI/ImportDogV6StagedTexturesUE58.py`：`inspect`（默认只读）/`import_preview`（真实48像素UE纹理导入）/`verify`（原生属性检查）/`preview_order`（既有唯一Profile的八槽只读顺序）。当前只有静态语法与缺图阻断真实验证；未执行任何修改模式。
- 通用 `CompleteImportedHeroIconProfilesMonolith.py` 已支持本英雄对应的唯一Profile路径和8个UI条目，使用临时目录中的 `IntegrationPlan.json` 与 `ProtectedFiles.json` 作为严格先决；不再开发平行Profile逻辑。
- 通用 `AuditImportedHeroSkillIconSmallSizes.py` 通过真实64/48源图生成技术报告和UMG审核规格；`PreviewImportedHeroIconsInVillageMonolith.py` 增补戌狗条件性入口，要求八张真实Texture2D和原生审查Widget存在才接受预览。

权威边界：保留五张V1原生Texture2D、唯一Placeholder UIProfile及既有三层插件体系；不擅自提交/推送主分支。以后若图像源可传至Windows，先校验来源、哈希和48/64尺寸，再用真实编辑器资产操作完成后续门禁；不以本记录作为构建、Cook、联机或人工艺术通过的证据。
