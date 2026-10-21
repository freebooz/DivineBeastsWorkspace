# DBAHeroPack_Rooster（酉鸡英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Rooster/`。仅持有酉鸡的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：保留五张V1图标；`V6/Textures/`新增八张原生48×48运行纹理。现有唯一UIProfile增补五项视觉条目，并接入当前授权八槽，保留九项旧条目的ID、名称和说明。
- `Content/UI/Review/`：八张原生64×64审核纹理及通过Monolith制作的`WBP_DBA_Rooster_SkillIconReviewV6`，用于明确标注“仅美术预览、不可施放”的审核。
- `SourceArt/UI/SkillIcons/V6/{Masters,Review64,Runtime48}/`：本次下载包中24份原始PNG，保持1254/64/48真实尺寸与原稿RGB，不以放大图替代原稿。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

2026-10-21实际库存为26份.uasset，含唯一UIProfile、V1与V6图标、审核页和既有外观/头像。16张V6纹理均已由Monolith导入、保存、重载、阻断审计，并与原始PNG逐像素比较。GPU、HUD及人工签收分项证据见`Saved/Validation/RoosterV6Import20261021/Delivery.json`；库存数量不代表Cook或联机验收。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。
