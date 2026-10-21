# DBAHeroPack_Dog（戌狗英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Dog/`。仅持有戌狗的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：既有5张V1图标、2026-10-20新增8张V6原生纹理及唯一技能UI Profile；纹理存在不等于V6机制已批准。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

2026-10-20内容包实际含17份.uasset：角色外观1、原型材质1、头像1、既有V1图标5、新增V6图标8、唯一技能UI Profile1。V6八图已通过UE5.8原生冷启动验证，但这不等于Cook、联机、HUD截图或人工艺术验收完成。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。


## 戌狗 V6 候选美术准备（2026-10-19历史状态）

- `Docs/DogV6_CandidateArtPlan_20261019.json`：G0身份核对后的24个A/B/C独立构图制作提示词、八项稳定技能编号与原定槽位、候选图和最终1024/64/48图源命名清单。
- `Docs/DogV6_CandidateReview.html`：离线审核工具，只有对应真实1024×1024图片存在时才允许选择A/B/C；八项都选好方可下载人工选择JSON。
- 2026-10-19当日图像生产接口未成功输出24候选；本条为初始历史记录。2026-10-20已改从英雄包内1254高清V1原图重新构图制作8张V6衍生母版与16张小尺寸图，随后通过真实UE5.8原生导入并增补唯一Profile；当前艺术验收和正式HUD运行仍待完成。
- 候选图计划归属 `SourceArt/UI/SkillIcons/V6/Candidates/`（尚未生成），正式文件计划归属 `Masters/`、`64/` 和 `48/`。后续必须逐张核查实际像素、散列值和48像素辨识度，再由真实Monolith接口导入与验收。

## 戌狗 V6 八技能美术冻结（2026-10-19）

- `Docs/DogV6_ArtFreeze_20261019.json` 保存用户本轮冻结的八技能视觉定义及稳定技能编号，不改变V5生产名称或GAS。
- `Docs/Implementation/GamePlatformAbilitySystem/DogV6_SevenStageExecution_20261019.md` 记录真实资源传输状态、阶段门禁和UE/HUD的待验收项目。
- 2026-10-19阶段性48/64 PNG曾仅存于ChatGPT临时包中；2026-10-20采用Windows工程原有1254高清源稿重新制作并真实导入，不以先前低清裁切包替换已导入源。

## 2026-10-20 戌狗V6原生技能图标集成（开发美术审核阶段）

- 八项真实1024派生母版、64审核PNG及48运行PNG位于 `SourceArt/UI/SkillIcons/V6/{Masters,64,48}/`；以戌狗原有1254高清图源重新构图，不宣称是八张独立AI绘画母版。`Docs/DogV6_DerivedArtManifest_20261020.json`登记来源、逐图SHA和技能身份。
- `Content/UI/Abilities/` 已由UE5.8原生Editor命令行导入8张新的48×48 `Texture2D`，并在新的引擎进程内再次加载验证。八图来源图像经过技术尺寸和颜色检查，`EnginePixelExport/DogV6_EnginePixelAudit.json`的八张引擎导出图与源PNG逐像素RGB差异为0。
- 原有唯一 `DA_DBA_Dog_PlaceholderUIProfile` 保留既有6项并增补8项V6视觉映射，共14条；只读美术顺序 `ArtPreviewAbilityIds` 锁定八项稳定ID，第四Slot4、第五Slot3。操作前保存了二进制备份。不授予任何未经批准的GAS技能，正式中文显示名保持V5。
- 源色/尺寸/原生导入及Profile冷加载通过；**美术人工通过、狗的专用审查Widget、Village/MainArena真实HUD截图、线上联机与服务器Cook隔离仍待验证**。完整实施证据见 `Docs/Implementation/GamePlatformAbilitySystem/DogV6_NativeIconIntegration_20261020.md`（相对于工作空间根）。
- 新增或复用工具：`Tools/Unreal/UI/AuthorDogV6FromVerifiedHighResSources.py`、`PrepareDogV6ArtIntegrationPlan.py`、`ImportDogV6StagedTexturesUE58.py`、`RunDogV6ProfileIntegrationUE58.py`、`VerifyDogV6EngineTextureColorsUE58.py`及通用64/48源PNG审核工具。原有五张V1图源和原生纹理不覆盖。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。
