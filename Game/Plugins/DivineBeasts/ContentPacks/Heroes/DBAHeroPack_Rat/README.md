# DBAHeroPack_Rat（子鼠英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Rat/`。仅持有子鼠的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：五张已导入技能图标纹理；文件名中的美术名称不代表正式技能身份已经批准。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

本轮文件系统观察为8份.uasset：外观配置1、原型材质1、图标5、头像1。现有形象仍属于开发/原型表现，不等于正式生肖模型。此数量不是UE重载、Cook、联机或人工艺术验收结果。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

## 2026-10-19 V6定稿图标导入

已从下载目录的 `SSLM_Rat_V6_ApprovedArt_HeroPack_ImportReady.zip` 接收用户指定定稿，24张PNG保存在 `SourceArt/UI/SkillIcons/V6/{Masters,48,64}`。Monolith MCP 0.23.0在锁定UE5.8正式工程创建8张 `Content/UI/Abilities/T_DBA_Rat_S01_*_V6` 至 `T_DBA_Rat_S08_*_V6` Texture2D，实际48×48、sRGB、无损UI压缩、NoMipmaps、NeverStream、Bilinear和Clamp。

8张纹理已保存、包重载、回读属性并逐像素核对原生导出RGB与原稿一致；18432个真实线性RHI像素颜色比较通过。旧V1、唯一PlaceholderUIProfile、Rat技能资产与V5名称共33份保护文件SHA保持不变。图标尚未绑定正式技能栏，未改技能授权和数值；当前包重载不等于另起编辑器冷启动、Cook或人工HUD验收。

正式清单：`Docs/Implementation/GamePlatformAbilitySystem/RatV6_FinalTextureImportManifest_20261018.json`；执行记录：同目录 `RatV6ApprovedArtImport_20261019.md`；本地证据：`Saved/Validation/RatV6Import20261019/`。本包既有 `Docs/MonolithGenerationManifest.json` 保留V1记录并追加V6记录。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。
