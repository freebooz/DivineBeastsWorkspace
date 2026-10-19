# DBAHeroPack_Ox（丑牛英雄表现内容包）

## 职责与归属

第三层DivineBeasts纯内容包，稳定挂载点 `/DBAHeroPack_Ox/`。仅持有丑牛的项目外观配置、原型材质、技能美术图标和头像；无C++模块，不持有权威技能实现、默认授予集合或伤害平衡真源。

## 已有内容与接入

- `Content/Characters/`：外观DataAsset与原型材质实例；由项目外观适配使用。
- `Content/UI/Abilities/`：保留原五张技能原型图标；新增 `V6/Textures/` 下八张 48×48 的原生 `Texture2D`（二维纹理），对应玄角撞、裂角、裂地踏、不动玄门、沉步、镇山护壁、顶回、立阵。全部使用稳定 `AbilityId`（技能标识），仅供项目客户端表现审核，不代表 V6 技能已经获得正式授权。
- `Content/UI/Portraits/`：一张头像纹理。
- [Monolith生成记录](Docs/MonolithGenerationManifest.json)：既有真实资产制作与引用证据。

历史阶段文件系统观察的8份 `.uasset` 数量仅代表当时原型资产快照，不包含随后新增的八张 V6 纹理及已有牛技能 UI 配置资产。本轮已使用独立 UE5.8 进程严格冷回读全部八张 `Texture2D`，验证压缩设置、UI 纹理组、sRGB、无 Mip 和禁止流式加载；结果为 `SSLM_OX_V6_COLD_REVIEW_RESULT 8 8 strict True`。此结果不代表真实战斗 HUD 试听/显示、Cook 或人工美术验收完成。

## V6 技能图标审核资源（2026-10-18）

- `SourceArt/UI/SkillIcons/V6/Review64/`：8 张真实 64×64 PNG，专供人工审核缩略图视觉质量。
- `SourceArt/UI/SkillIcons/V6/Runtime48/`：8 张真实 48×48 PNG，对应已导入 Unreal Editor（虚幻编辑器）的正式目录纹理源。
- `SourceArt/UI/SkillIcons/V6/OxV6_64_48_SourceManifest.json`（美术来源清单）：记录八个技能的稳定标识、真实槽位、64/48 文件路径和 SHA-256（文件指纹）。
- `/DBAHeroPack_Ox/UI/Abilities/V6/Textures/`（客户端纹理挂载路径）：八张原生 `Texture2D`，独立冷回读全部通过，日志位于 `Game/Saved/Logs/SSLM_OxV6HeroPack_8Texture_ColdVerify_20261018.log`（UE5.8 验证日志）。

**审核限制**：这 16 张小图是从丑牛原有五张 1254 像素高清美术衍生的临时审核版本，不是聊天中另行生成的八张原画。部分 V6 复杂技能构图仍需重绘；本轮在既有唯一 `DA_DBA_Ox_PlaceholderUIProfile`（牛技能界面配置）中**只增补八条客户端表现元数据**，保留原六条占位技能，合计十四条，**没有修改GAS技能授予**。真实已授权技能才能进入战斗HUD，未批准的八项V6不会凭借图标自动可施放。

## 数据身份与资源生命周期

正式技能定义与服务器授权归DBAGameplay及既有平台能力系统。能力栏通过GamePlatformData资源租约异步加载UIProfile及图标，换英雄/解绑时仅释放自身租约。开发UIProfile仍在独立开发目录；本包不得通过复制开发编号来填充未经批准的正式UIProfile。
正式名称、AbilityId、数值与默认技能集的批准状态以 `Docs/Implementation/ZodiacAbilityAssetInventory.json` 和后续明确批准记录为准。2026-10-11资产迁移授权仅针对开发验证，不能扩大为生产批准。

2026-10-19唯一牛UIProfile的八条V6软图标映射已经用UE5.8原生DataAsset真实写入并独立冷重载通过；结果 `SSLM_OX_V6_PROFILE_COLD_VERIFY_OK 8 placeholders 6 profileEntries 14 unapprovedGrants 0`。现有 `DivineBeastsAbilityBarViewModel`（技能栏视图模型）读取权威Loadout的AbilityId，经GamePlatformData加载此Profile/图标；只有服务器实际授予V6技能后才会显示对应正式槽，禁止伪造八个可施放的战斗技能。

## Cook与验收

当前内容为客户端表现，服务器排除须按统一服务器策略及真实容器检查；将来增加权威必需资源时必须先拆分精确目录并更新回归，不能靠历史整包规则误删。
运行 `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory` 核对库存，`--release` 检查正式交付前置；缺批准时保留失败/未完成状态。统一场景和证据入口见 `Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json`。二进制资产只由真实UE工具生成/保存，UI资产制作使用Monolith。
