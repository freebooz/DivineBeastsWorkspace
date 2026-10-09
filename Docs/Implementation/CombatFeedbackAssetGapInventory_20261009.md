# 神兽联盟战斗打击反馈资产缺口与首批落地清单

日期：2026-10-09。范围为 DivineBeastsWorkspace 当前游戏插件仓库真实文件，只统计本次已检查的 DBAClient、DBAArena、丑牛、寅虎、卯兔三个英雄内容包。文件存在不代表 UE5.8 AssetRegistry 类型、资源软引用、Cook 或显示效果已经验收。

## 一、已检查的真实文件

### 丑牛（DBAHeroPack_Ox）
- 角色外观定义：Content/Characters/DA_Appearance_Zodiac_Ox.uasset。
- 原型材质实例：Content/Characters/Materials/MI_Zodiac_Ox_Prototype.uasset。
- UI 技能资源共5个文件：Content/UI/Abilities/ 目录下 BasicAttack_HornStrike、Active01_HornCharge、Active02_EarthPulse、Passive_Stonehide、Ultimate_MountainRupture 对应的 T_DBA_Ox_*.uasset。
- 本范围未发现名称对应的受击Montage、Niagara技能命中特效、SFX命中材质音或正式HitFeedback Profile。

### 寅虎（DBAHeroPack_Tiger）
- 角色外观定义：Content/Characters/DA_Appearance_Zodiac_Tiger.uasset。
- 原型材质实例：Content/Characters/Materials/MI_Zodiac_Tiger_Prototype.uasset。
- UI 技能资源共5个文件：Content/UI/Abilities/ 目录下 BasicAttack_ClawStrike、Active01_Pounce、Active02_CrescentRake、Passive_RegalSpirit、Ultimate_RoarStorm 对应的 T_DBA_Tiger_*.uasset。
- 本范围未发现名称对应的受击Montage、Niagara技能命中特效、SFX命中材质音或正式HitFeedback Profile。

### 卯兔（DBAHeroPack_Rabbit）
- 角色外观定义：Content/Characters/DA_Appearance_Zodiac_Rabbit.uasset。
- 原型材质实例：Content/Characters/Materials/MI_Zodiac_Rabbit_Prototype.uasset。
- UI 技能资源共5个文件：Content/UI/Abilities/ 目录下 BasicAttack_MoonStrike、Active01_MoonLeap、Active02_JadeWard、Passive_JadeSpirit、Ultimate_LunarBloom 对应的 T_DBA_Rabbit_*.uasset。
- 本范围未发现名称对应的受击Montage、Niagara技能命中特效、SFX命中材质音或正式HitFeedback Profile。

上述文件名是当前工程文件级证据，不能从图标资源名反推出服务器可信技能ID；技能实际名称、类别与可用等级须以权威 AbilityDefinition / AbilitySet / OwnerOnly 授予快照为准。

## 二、真实资源生成归属

1. GamePlatformPresentationCore：使用现有 UGamePlatformHitFeedbackProfile 创建跨游戏通用的轻击、重击、技能命中预设；Profile 必须设置合法 LogicalId / DataVersion，CameraShakeClass 与 HitFlashOverlayMaterial 只通过 Client Bundle 软引用加载。不得为UI图标再创建第二套技能身份。
2. DBAClient / Definitions：发布项目 UDivineBeastsCombatFeedbackCatalog 主资产，只记录现有权威 HeroDefinitionId + AbilityDefinitionId 到合法 ProfileDefinitionId、VFXDefinitionId、SFXDefinitionId 的关系；同键重复或空Profile拒绝发布。正式资产经GamePlatformData统一数据租约加载。
3. GamePlatformAnimationClient / GamePlatformCameraClient：负责通用Overlay闪白、局部视觉顿帧和CameraShake执行；具体 Overlay 材质与Shake曲线由 UE5.8 真正制作，保持短促、无全屏闪白和UI抖动。
4. GamePlatformVFXClient / GamePlatformSFXClient：负责执行Niagara和音效请求、缓存、并发/预算及资源生命周期；实际十二生肖视觉和声音内容归各 DBAHeroPack_* 项目内容包，不复制平台执行器。
5. DBAArena客户端：当前按已确认 HeroDefinitionId、SourceAbilityId、Avatar代次及 OwnerOnly Loadout 快照挑选已加载Profile；未加载时安全回退。不允许项目目录与输入代码进行 LoadSynchronous。
6. Gameplay服务器：技能结算、碰撞、硬直、真实击退和属性变化继续由Combat/GAS授权实现。视觉Profile参数仅在客户端生效。

## 三、正式验收门槛

- 创建并真实保存三种可调参考Profile，并可在编辑器重新加载和查询GamePlatformDefinition主资产ID；确认软件并非仅生成JSON或空占位蓝图。
- 用编辑器实际查询并核对丑牛、寅虎、卯兔的已授予主动技能与普通攻击ID，再写入 Catalog；缺失的权威定义须记为待交付，而不能用图标文件名代替。
- 导入真实目标闪白Overlay、CameraShake曲线、Niagara接触粒子和SFX分层音色；在编辑器运行时确认各资源分别属于正确内容插件，并在Cook/AssetRegistry复核软引用。
- 使用本地控制台变量 gp.Combat.HitstopOverrideFrames 进行 -1/0/3/6帧的同技能对比；验证输入缓冲、受击动画、局部顿帧、材质恢复和UI稳定。
- Dedicated Server + 双客户端复核多角色可见性、延迟和丢包；RootMotion技能采用保守跳过视觉停顿，直到真实移动分离测试通过；5v5时测CPU/GPU、Niagara与SFX实例数。
- 当前尚无上述完整实机验收证据：不得将本目录、测试源码或已有UI图标视为3A打击反馈已经交付。

核验依据：Docs/Implementation/CombatFeedbackWorkOrders_20261009.md（P0—P8工单）及对应三个英雄插件的Content目录真实文件。
