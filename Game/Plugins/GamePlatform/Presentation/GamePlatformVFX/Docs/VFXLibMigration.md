# VFXLibMigration（VFX Lib 第一批迁移说明）

版本：1.0｜2026-09-30

来源：外部 VFX 资产库 F:\VFX Lib。
目标：只提取跨游戏通用技术能力，不把 FrostMage（冰霜法师）、PetalBloom（花瓣绽放）、生肖、技能名或项目美术主题下沉到 GamePlatform（游戏平台层）。

## 1. 第一批迁移范围

| 母版能力 | 来源证据 | 平台化结果 | 当前状态 |
| --- | --- | --- | --- |
| Charge / Cast（蓄力 / 施法） | ChargeMotionFinal.hlsl | Hash、Orbit、SmoothWindow | HLSL 已落地 |
| Projectile + Trail（弹体 + 拖尾） | ProjectileMotionFinal.hlsl | Rotate2D、TrailWidth、Source/Target/Speed 参数 | HLSL + 参数契约已落地 |
| Impact + ShockRing（命中 + 冲击环） | ImpactMotionFinal.hlsl / ShockRing 源纹理 | BallisticOffset、ExpandingRadius、冲击环源图 | HLSL + SourceArt 已落地 |
| Orbit / Petal / Debris（环绕 / 漂浮 / 碎片） | PetalMotion.hlsl / NovaMotionFinal.hlsl | SoftFloat、GoldenAngleDirection、雾/光晕/闪光源图 | HLSL + SourceArt 已落地 |

## 2. 已迁移平台源文件

通用 Shader / HLSL：Shaders/Private/GamePlatformVFXCommonMotion.ush
稳定虚拟包含路径：/Plugin/GamePlatformVFX/Private/GamePlatformVFXCommonMotion.ush

当前函数：
- GPVFX_Hash01：稳定伪随机。
- GPVFX_Rotate2D：二维旋转。
- GPVFX_SmoothWindow：淡入淡出时间窗。
- GPVFX_OrbitOffset：环绕偏移。
- GPVFX_ExpandingRadius：扩张半径。
- GPVFX_BallisticOffset：弹道碎片偏移。
- GPVFX_TrailWidth：拖尾宽度曲线。
- GPVFX_SoftFloat：轻盈漂浮。
- GPVFX_GoldenAngleDirection：黄金角径向分布。

这些函数是表现数学工具，不得用于 Gameplay（玩法）命中、范围、随机或权威状态。

## 3. 通用 Niagara 参数名

代码：Public/Types/GamePlatformVFXCommonParameters.h 与 Private/Types/GamePlatformVFXCommonParameters.cpp。

| 参数 | 类型建议 | 中文用途 |
| --- | --- | --- |
| User.PrimaryColor | Color | 主色 |
| User.SecondaryColor | Color | 次色 |
| User.CoreColor | Color | 高亮核心色 |
| User.Intensity | Float | 表现强度 |
| User.Duration | Float | 表现时长 |
| User.Radius | Float | 表现半径 |
| User.Length | Float | 视觉长度 |
| User.Width | Float | 视觉宽度 |
| User.Speed | Float | 表现运动速度 |
| User.Seed | Integer | 表现随机种子 |
| User.SourcePosition | Position | 来源位置，LWC 安全 |
| User.TargetPosition | Position | 目标位置，LWC 安全 |
| User.Direction | Vector | 中立方向 |
| User.Scale | Vector | 表现缩放 |

注意：Radius/Length/Width/Duration 只是 VFX 参数。真实技能范围、碰撞、伤害和时序仍由 Gameplay/Ability/Combat 权威数据决定。

## 4. 已迁移 SourceArt（源美术）

源素材只放 SourceArt/VFXLib/Common，不会被当作运行时 Content 或 Shipping Cook 资产。

| 新文件 | 原始用途 | 平台化用途 |
| --- | --- | --- |
| T_GPVFX_Glint_Source.png | 冰霜闪光 | 通用 Glint（闪光） |
| T_GPVFX_ShockRing_Source.png | 冰霜冲击环 | 通用 ShockRing（冲击环） |
| T_GPVFX_SoftCore_Source.png | 冰霜核心 | 通用 SoftCore（柔光核心） |
| T_GPVFX_Noise_Source.png | 冰霜噪声 | 通用 Noise（噪声） |
| T_GPVFX_Mist_Source.png | 花瓣地雾 | 通用 Mist（雾） |
| T_GPVFX_Halo_Source.png | 花瓣光晕 | 通用 Halo（光晕） |

SourceManifest.json 保存迁移后的 SHA-256 和字节数，用于人工追溯。

未迁移到平台层：Frost Dendrite（霜晶枝状）、Ice Crack / Ice Normal（冰裂 / 冰法线）、Ice Shard / Ice Lance（冰片 / 冰枪）、Petal（花瓣）、Rose/Jade（玫瑰/玉色）主题材质、Hero Plume（特定主角羽流）。这些具有明显美术主题，只能作为 DivineBeasts（神兽联盟项目层）候选素材。

## 5. Niagara 资产状态

本批没有伪造 .uasset。
建议后续在 Unreal Editor 中将上述函数封装为：FXM_GPVFX_ChargeGather、FXM_GPVFX_Orbit、FXM_GPVFX_ProjectileSpiral、FXM_GPVFX_RibbonTrail、FXM_GPVFX_RadialBurst、FXM_GPVFX_GroundShockRing、FXM_GPVFX_DebrisBurst、FXM_GPVFX_SoftFloat。
再组合技术模板：FXS_GPVFX_Cast_Charge、FXS_GPVFX_Projectile_Core、FXS_GPVFX_Projectile_Ribbon、FXS_GPVFX_Impact_Radial、FXS_GPVFX_Impact_GroundRing、FXS_GPVFX_Area_ExpandingRing、FXS_GPVFX_Orbit_Rise。

这些 FXS_* 只能是技术母版，不是玩家最终看到的生肖技能。

## 6. 项目层复用原则

GamePlatformVFX 只提供“怎么播”和通用表现原语。DivineBeasts 项目层负责 HeroDefinitionId + AbilityId + SkinId + Semantic → Presentation Catalog → Presentation.DBA.Hero.<Hero>.<Ability>.<Phase> → DBAHeroPack_* 真实内容 → GamePlatformVFX 执行。

禁止平台层出现 Rat/Ox/Tiger/Rabbit/Dragon 等生肖名、Frostbolt/FrostNova/PetalBloom 来源项目技能名、DivineBeasts AbilityId 或 Moba 伤害/控制规则。

## 7. 性能迁移规则

1. 复杂 Layer 分支优先拆成多个职责单一 Emitter（发射器）。
2. 高频 pow/exp 仅在视觉收益明确时保留。
3. 远距离/低质量优先削减次级 Ribbon、Light、Debris 和雾。
4. Effect Type（Niagara 特效类型）必须承担 Scalability（可伸缩性）。
5. 5v5 场景不得以单技能审核性能代替同屏预算验证。

## 8. 验收状态

已完成：通用 HLSL 去主题化迁移、Shader 虚拟路径接入源码、通用 Niagara 参数名契约、通用 SourceArt 迁移与 SHA-256 清单、C++ Automation 参数名测试源码、十二生肖复用矩阵设计。

未完成，不能标记通过：Niagara Module Script .uasset、通用 FXS 技术母版 .uasset、VFX Definition 真实资产、Review Map、Client Cook / Server Leak Audit 实际工件、5v5 / Android GPU Profile。
