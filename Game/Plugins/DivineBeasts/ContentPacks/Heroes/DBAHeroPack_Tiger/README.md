# DBAHeroPack_Tiger（寅虎英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Tiger/`。仅持有寅虎的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：五张已导入技能图标纹理；文件名中的美术名称不代表正式技能身份已经批准。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

本轮文件系统观察为8份.uasset：外观配置1、原型材质1、图标5、头像1。现有形象仍属于开发/原型表现，不等于正式生肖模型。此数量不是UE重载、Cook、联机或人工艺术验收结果。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

## V6八技能已选源图（2026-10-18）

用户选取1A/2B/3C/4A/5A/6B/7B/8B构图，本包的 `SourceArt/UI/SkillIcons/V6/` 已保存8张独立1024×1024候选原稿、8张1024母版、8张64×64审核PNG及8张48×48正式尺寸PNG（均属源艺术文件）；`Review/` 内存小尺寸检查图和被拒原画留底。详见 `Docs/Implementation/GamePlatformAbilitySystem/TigerV6HeroPackSourceArtDelivery_20261018.md`。
**当前V6尚无原生UE Texture2D.uasset，仍保留本包原有5张V1纹理及唯一正式Tiger UI Profile不动。** HWebCodex尚无可调用Monolith MCP Provider且UE5.8编辑器未运行，后续由Monolith创建的原生贴图需另做保存、冷重载、客户端显示、服务器Cook剥离与用户艺术验收。不得把源PNG放在SourceArt中的事实当作生产HUD已接入。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。

## 寅虎 V6 图标原生纹理导入（2026-10-19）

已使用 Monolith MCP 导入八套所选V6技能图标到 `Content/UI/Abilities/`（原生Texture2D，真实48×48），逐项保存、设置UI压缩/sRGB/无Mip/Bilinear/Clamp，且在独立UE5.8进程冷重载通过。1024源原画与64/48PNG仍在 `SourceArt/UI/SkillIcons/V6/`。原五张V1纹理及现行 `DA_DBA_Tiger_PlaceholderUIProfile` 不覆盖；八项V6正式技能授权、美术终审、UI Profile绑定、真实HUD与Cook尚未完成。

证据：`Docs/Implementation/GamePlatformAbilitySystem/TigerV6UE58NativeIconImportReport_20261019.md`（原生导入与验证报告），`Docs/MonolithGenerationManifest.json` 的 `TigerV6SelectedSkillIconImport`（V6资产清单）。
