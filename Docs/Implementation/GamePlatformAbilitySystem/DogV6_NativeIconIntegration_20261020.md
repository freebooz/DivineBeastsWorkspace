# SSLM 戌狗 V6 八技能图标 UE5.8 原生实施记录（2026-10-20）

## 范围与身份冻结

英雄身份：`Hero.Zodiac.Dog`，戌狗·守阙天犬；八项技能稳定编号、视觉主体、1024/64/48纹理命名和输入顺序见 `Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog/Docs/DogV6_ArtFreeze_20261019.json`。

八槽真实显示顺序：`Primary → Slot1 → Slot2 → Slot4 → Slot3 → Slot5 → Slot6 → Slot7`。第四格终极技能为Slot4，第五格移动技能为Slot3，不按数字自然排序。V6设计名称仅用于本阶段美术审核；四个V6改名仍未覆盖已批准V5生产显示名。第八项`loyal_heal`沿用稳定身份，不代表新V6技能要治疗。

## 已在Windows工作空间真实完成

1. **八项美术设计冻结**：经 V6 审稿源、V5批准表和旧源哈希核对。戌狗固有被动“守阙”不占八主动槽。
2. **八张高清衍生美术图源**：`Tools/Unreal/UI/AuthorDogV6FromVerifiedHighResSources.py` 读取戌狗已有的5张1254×1254 V1原生美术PNG，并按八项机制分别重构画面，新增门盾、犬牙方向、双爪、巨门、跃进、誓约双目标元素。输出 `SourceArt/UI/SkillIcons/V6/Masters/` 的8张1024 PNG、`64/` 的8张审核PNG、`48/` 的8张真实运行PNG。此为**派生重构高清稿，并非八张独立AI生成的新母版**；其美术质量仍待人工签收。未覆盖任何V1。
3. **真实UE5.8纹理导入**：`Tools/Unreal/UI/ImportDogV6StagedTexturesUE58.py`经命令行UE Editor PythonScriptCommandlet执行`probe_ue`、`import_preview`、`verify`。在唯一戌狗内容包 `Content/UI/Abilities/` 创建8张真实`Texture2D`（命名为`T_DBA_Dog_S01_`至`S08_..._V6`），读取原生AssetRegistry全部确认为48×48、UI纹理组、`TC_EDITOR_ICON`、sRGB、NoMipmaps；另次重新启动真实引擎后冷加载验证通过。
4. **唯一UI Profile接线**：沿用原生`DA_DBA_Dog_PlaceholderUIProfile.uasset`（未新建第二个同主资产身份），保留6项既有占位条目，增补8项按稳定AbilityId索引的V6图标条目，总数14。运行`Tools/Unreal/UI/RunDogV6ProfileIntegrationUE58.py`先inspect、再apply、再独立UE冷启动verify，均取得原生日志与报告；备份位于`Saved/Validation/DogV6Staging20261019/Backups/Dog/`，同阶段还单独备份了只读排序变更前的Profile。
5. **八槽只读UI ViewModel配置**：为上述唯一Profile的`ArtPreviewAbilityIds`写入八个完整稳定ID，UE引擎保存后反向读取确认。代码仍复用`UDivineBeastsAbilityBarViewModel`与`UDivineBeastsAbilityBarPanel::PreviewHeroIcons`，不制造新的可施放技能，也不改变任何GAS授权。未加载权威Loadout的新VM保持零授权槽，既有生命周期/资源租约逻辑不被替换。**真正的玩家选择角色→HUD画刷显示是下一阶段尚待实机核验的运行验收**。
6. **小尺寸与颜色技术验收**：`AuditImportedHeroSkillIconSmallSizes.py`对8张64+8张48源PNG共16份进行真实像素尺寸和SHA验证，且未发现完全重复或dHash高相似配对。修复旧Pillow 10.4环境`get_flattened_data`兼容问题。原生引擎`VerifyDogV6EngineTextureColorsUE58.py`导出八张48像素Texture2D，对照源PNG进行逐像素RGB比较；8/8通过，最大通道差异为0。
7. **新手村与战斗HUD**：尚无真正的`L_Village_Start`或`MainArena` UI运行截图、48像素DPI/冷却蒙层实机目测及真实Client/Server Cook证据，**不得标成第7步完成**。已确认真实`/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD`蓝图存在、`L_Village_Start.umap`存在、戌狗唯一Profile可加载；但戌狗专属`WBP_DBA_Dog_SkillIconReviewV6`审核蓝图尚不存在，且本轮HWebCodex未注册可直接调用的Monolith服务，无法在已连接图形编辑器中按审核流程生成HUD截图。启动任何前端联机校验前仍须逐一确认真实Village与MainArena服务端战斗构建/复制合同版本一致。

## 原生证据与资产保护

- `Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog/Docs/DogV6_DerivedArtManifest_20261020.json`：逐图源PNG SHA、1024/64/48尺寸与V1派生来源的制作快照（生成时原生资产尚未导入，此字段不代表最终阶段状态）。
- `Docs/DogV6_StagingManifest.json`（戌狗内容包内）：为本轮图源准备的身份与SHA合同。
- `Saved/Validation/DogV6Staging20261019/{IntegrationPlan.json,ProtectedFiles.json,SmallSizeAudit.json,Dog_Source64_48.png,Profiles_inspect.json,Profiles_apply.json,Profiles_verify.json}`：映射、旧V1 SHA保护、48/64尺寸与唯一Profile回归。
- `Saved/Validation/DogV6Staging20261019/UEImportEvidence/{DogV6_import_preview.json,DogV6_verify.json,DogV6_preview_order.json}`：UE原生纹理保存/冷启动读取与八槽UI排序记录。
- `Saved/Validation/DogV6Staging20261019/EnginePixelExport/DogV6_EnginePixelAudit.json`：八张原生引擎导出PNG与源PNG的RGB逐像素比对报告。
- `Saved/Validation/DogV6Art20261020/DogV6_RealPixelSizeAudit.png`：本轮刚生成的8项64/48像素原尺寸审核图，不是游戏内截图。
- V1原有5张PNG和5张`.uasset`全部保持原SHA-256；`Tests/Architecture/TestDogV6ArtFreeze.py`四项回归全部通过。原始Profile保存了二进制回退备份。
- 实际UE引擎使用锁定5.8.0，原生操作采用UE Editor PythonScriptCommandlet（本轮未取得Monolith MCP直接调用通道）；没有以文本修改、假后缀或拼贴截图冒充真正的原生UE验收。

## 必需的后续验收门禁

由能连接Monolith的UE5.8图形编辑器打开原有技能栏、生成/核验狗的16图原生审核Widget，并在单机只读预览模式调用`PreviewHeroIcons(Hero.Zodiac.Dog)`，检查八槽图片、名称、冷却遮罩、禁用、底部65像素高度与上下3像素条。随后在真实`L_Village_Start`和`MainArena`分别留存UE原生截图与日志；需要在线准入时先核对Client/Village/MainArena的构建及复制合同，并验证专服Cook剥离纯客户端UI资源。由人工最后确认图标达到电影质感和48像素辨识度后，方可将此套美术资产签为正式图标；V6机制与V5显示名称是否投产另走审批。

本轮修改保留在当前本地工作树中，**未按本任务擅自Git提交、推送或合并主分支**。
