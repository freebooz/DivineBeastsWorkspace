# DBAHeroPack_Snake（巳蛇英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Snake/`。仅持有巳蛇的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：五张已导入技能图标纹理；文件名中的美术名称不代表正式技能身份已经批准。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

上述早期记录为8份.uasset；其后现有唯一PlaceholderUIProfile以及下方新增V6图标分别保留原身份和独立记录。现有形象仍属于开发/原型表现，不等于正式生肖模型。文件数量不是Cook、联机或人工艺术验收结果。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

## 2026-10-19 V6独立源图导入

从用户指定下载包 `SSLM_SnakeV6_ImportReady.zip` 接收24张PNG：8份1254×1254源稿归 `SourceArt/UI/SkillIcons/V6/SelectedMasters`，48及64像素版本归 `Final/48` 和 `Final/64`。此前程序化草稿、旧V1及现有Profile保留原状。

Monolith MCP 0.23.0在锁定UE5.8正式工程创建8张 `UI/Abilities/T_DBA_Snake_S01_*_V6` 至 `T_DBA_Snake_S08_*_V6` Texture2D，实际48×48，sRGB、无损UI压缩、NoMipmaps、NeverStream、Bilinear与Clamp。已保存、重载及执行真实导入阻断审计；UE源像素导出RGB与PNG完全一致，18432个浮点RHI像素比较通过。旧包资源、草稿、权威技能和命名/规划共58份文件SHA保持不变。

当前是独立图像资源导入，尚未改动唯一UIProfile、V5名称、技能授予或数值。包内V6候选名与最终美术质量仍保留人工审核边界，未完成HUD、Cook或独立编辑器冷启动验收。清单为 `Docs/Implementation/GamePlatformAbilitySystem/SnakeV6SelectedTextureImportManifest_20261019.json`，同目录 `SnakeV6SelectedArtImport_20261019.md` 记录执行；证据位于 `Saved/Validation/SnakeV6Import20261019/`。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。
