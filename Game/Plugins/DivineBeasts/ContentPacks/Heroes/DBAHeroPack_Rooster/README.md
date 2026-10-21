# DBAHeroPack_Rooster（酉鸡英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Rooster/`。仅持有酉鸡的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：五张已导入技能图标纹理；文件名中的美术名称不代表正式技能身份已经批准。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。
- `SourceArt/UI/SkillIcons/V6/Delivery/SSLM_RoosterV6_8SkillIcons_ReadyForUE58.zip`：2026-10-21找回并按原SHA校验的完整正式源包，包含八张1254母稿、八张64审核图和八张48运行图。既有导入器可在接收目录缺包时直接读取；未在本轮生成原生V6纹理。

后续源包校验、文件交付及真实引擎接入边界见工作空间 `Docs/Implementation/GamePlatformAbilitySystem/RoosterV6_SourceDelivery_20261021.md`，不能把源包存在视为Profile绑定或游戏运行验收通过。

本次基线文件系统观察为9份.uasset：外观配置1、原型材质1、技能界面配置1、图标5、头像1。现有形象仍属于开发/原型表现，不等于正式生肖模型。此数量不是UE重载、Cook、联机或人工艺术验收结果。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。本包已有 `Content/UI/Abilities/DA_DBA_Rooster_PlaceholderUIProfile.uasset`；V6只允许备份后增补该现有配置。独立开发目录仍有开发UIProfile，须在真实引擎中检查全局主资产身份，不能复制开发编号或新建同英雄重复主资产。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。
