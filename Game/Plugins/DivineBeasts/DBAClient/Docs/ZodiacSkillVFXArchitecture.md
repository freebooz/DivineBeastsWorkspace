# 十二生肖技能 VFX（视觉特效）三层架构设计规范

版本：1.0
适用工程：DivineBeastsWorkspace（神兽联盟工作空间）
归属：DBAClient / DivineBeastsPresentationRuntime（项目表现运行模块）与十二生肖 ContentPacks（内容包）

## 1. 总体原则

神兽联盟的技能 VFX 不按“每个生肖建立一套特效系统”实现，而按“统一平台原语 + MOBA 中立语义 + 项目视觉语言与内容映射”组合。

    DivineBeasts（神兽联盟项目层）
    HeroDefinitionId / AbilityId / SkinId / Hero VFX Profile / Catalog / Content Pack
                               ↓
    MobaPresentation（MOBA表现语义层）
    Cast / Release / Projectile / AreaWarning / Hit / Critical / Heal / Control / Status
                               ↓
    GamePlatformVFX（游戏平台VFX基础层）
    Instant / Attached / Projectile / Beam / Area / Shield / Portal / Trail / World / Composite
                               ↓
    Niagara / Material / Decal / Mesh / Light

职责：
- GamePlatformVFX：负责“怎么播”。
- MobaPresentation：负责“发生了什么”。
- DivineBeasts：负责“看起来是什么”。

## 2. 权威边界

VFX 永远不是 Gameplay 权威真源。伤害、治疗、技能真实范围、命中、碰撞、控制时长、护盾值、权威位移、技能开始/取消/结束都来自 Gameplay/Ability/Combat。VFX 只读取表现上下文显示这些事实，禁止通过 Niagara 粒子尺寸反推真实技能范围。

## 3. Hero VFX Profile（英雄VFX视觉配置）

每个生肖英雄必须有一个稳定默认 Profile。Profile 描述视觉语言，不持有 UObject（虚幻对象）、AssetPath（资产路径）或具体 Niagara 资源。

字段：
- HeroDefinitionId（英雄定义编号）
- ProfileId（视觉特效配置编号）
- ShapeLanguageId（形状语言编号）
- MotionLanguageId（运动语言编号）
- EnergyLanguageId（能量语言编号）
- MaterialLanguageId（材质语言编号）
- SignatureMotifId（标志性视觉符号编号）
- ImpactLanguageId（命中语言编号）
- DissipationLanguageId（消散语言编号）

默认命名：Presentation.VFX.Hero.Zodiac.<Hero>.Default

## 4. 十二生肖视觉 DNA

### 4.1 子鼠·影牙 Rat
Shape：Sharp / Thin / Crescent（尖锐、细长、弧刃）
Motion：Dash / Zigzag / Afterimage（突进、折线、残影）
Energy：ShadowSilver（暗影银光）
Material：DarkMetal / ShadowMist（暗色金属、影雾）
Signature：FangSlash（牙刃切痕）
Impact：CrossCut（十字短促切割）
Dissipation：FastCollapse（快速内收消散）
主要原语：Trail、Instant、Attached、Composite

### 4.2 丑牛·玄角 Ox
Shape：Broad / Ring / Horn（宽厚、环形、牛角）
Motion：Charge / HeavyPulse（冲锋、重脉冲）
Energy：EarthForce（厚重地脉能量）
Material：Rock / Dust（岩质、尘土）
Signature：HornShockwave（牛角冲击波）
Impact：GroundFracture（地面震裂）
Dissipation：DustSettle（尘浪沉降）
主要原语：Attached、Trail、Area、Instant、Composite

### 4.3 寅虎·白君 Tiger
Shape：Claw / Diagonal / Crown（爪痕、斜切、王者轮廓）
Motion：Pounce / Burst（扑击、爆发）
Energy：WhiteGold（白金能量）
Material：FurLight / EnergyStripe（光纹、能量虎纹）
Signature：TigerClaw（虎爪）
Impact：RoarBurst（虎啸冲击）
Dissipation：StripeScatter（条纹粒子散开）
主要原语：Trail、Instant、Area、Composite

### 4.4 卯兔·玉灵 Rabbit
Shape：MoonArc / Ring / Petal（月弧、环、花瓣）
Motion：Leap / Float（跳跃、轻盈上浮）
Energy：JadeMoon（玉光、月光）
Material：TranslucentJade（半透明玉质）
Signature：JadeMoonArc（玉色月弧）
Impact：SoftRing（柔和环状冲击）
Dissipation：JadePetalRise（玉屑/花瓣上升）
主要原语：Attached、Trail、Area、Shield、Composite

### 4.5 辰龙·苍龙 Dragon
Shape：Spiral / Scale / DragonCurve（螺旋、龙鳞、长曲线）
Motion：Orbit / Dive / Ascend（盘旋、俯冲、升腾）
Energy：AzureCloud（苍青云气）
Material：Scale / Cloud / EnergyBody（龙鳞、云气、能量体）
Signature：DragonHeadCloud（龙首与云纹）
Impact：CloudThunderBurst（云雷大范围爆发）
Dissipation：CloudSpiralFade（云气螺旋消散）
主要原语：Area、Trail、Beam/Projectile、Instant、Composite
普通技能只使用局部龙纹/龙爪/龙首；完整龙形优先保留给终极技能和关键演出。

### 4.6 巳蛇·幽鳞 Snake
Shape：S-Curve / Scale / Coil（S形、鳞片、盘绕）
Motion：Winding / Tracking（蛇行、追踪）
Energy：MysticScale（幽光鳞能量）
Material：IridescentScale（虹彩鳞片）
Signature：SerpentCurve（灵蛇曲线）
Impact：CoilBurst（盘绕式爆发）
Dissipation：ScaleFade（鳞光渐隐）
主要原语：Projectile、Beam、Trail、Attached、Composite

### 4.7 午马·雷蹄 Horse
Shape：Line / Hoof / LightningFork（直线、蹄印、雷叉）
Motion：Sprint / Charge（高速奔驰、冲锋）
Energy：Thunder（雷电）
Material：ElectricArc（电弧）
Signature：ThunderHoof（雷蹄）
Impact：LightningRing（雷电冲击环）
Dissipation：ArcGrounding（电弧接地消散）
主要原语：Attached、Trail、Area、Instant、Composite
性能重点：高速移动中限制重复地面蹄印与电弧生成频率。

### 4.8 未羊·玉角 Goat
Shape：Halo / Horn / Dome（光环、玉角、穹顶）
Motion：Expand / Rise（扩散、上升）
Energy：JadeGuardian（玉质守护能量）
Material：TranslucentShield（半透明护罩）
Signature：JadeHornRune（玉角符纹）
Impact：ShieldRipple（护盾局部波纹）
Dissipation：JadeShardFade（玉屑消散）
主要原语：Shield、Attached、Area、Composite

### 4.9 申猴·灵猴 Monkey
Shape：Arc / StaffCircle / Afterimage（棍弧、圆扫、残影）
Motion：Trick / Flip / MultiDash（戏法、翻转、多段突进）
Energy：GoldenStroke（金色笔触）
Material：EnergyInk / Smoke（能量墨迹、烟雾）
Signature：StaffArc（棍扫圆弧）
Impact：MultiHitPop（多段短促命中）
Dissipation：SmokePop（烟雾弹式消散）
主要原语：Trail、Instant、Attached、Composite
允许有限 Variant（变体），但必须保持可读性一致。

### 4.10 酉鸡·金鸣 Rooster
Shape：Fan / Feather / RadialWave（扇形、羽毛、环波）
Motion：Spread / Pulse（展开、脉冲）
Energy：GoldenDawn（金色晨光）
Material：FeatherLight（金羽光）
Signature：GoldenFeather（金羽）
Impact：SonicRing（鸣叫音波环）
Dissipation：FeatherScatter（羽光散落）
主要原语：Area、Projectile、Instant、Composite

### 4.11 戌狗·天犬 Dog
Shape：Mark / Paw / GuardianRing（标记、足迹、守护环）
Motion：Track / Leap（追踪、扑跃）
Energy：SkyGuardian（天犬守护光）
Material：SpiritLight（灵体光）
Signature：TrackingMark（追猎标记）
Impact：GuardianBiteBurst（守护扑击爆发）
Dissipation：TrailMarkFade（追踪印记渐隐）
主要原语：Attached、Trail、Projectile、Instant、Composite

### 4.12 亥猪·玄鬃 Boar
Shape：WideArc / PressureRing / Mane（宽弧、压力环、鬃毛）
Motion：Ram / Burst / Spread（冲撞、爆发、扩散）
Energy：HeavyForce（厚重冲击能量）
Material：Dust / PressureDistortion（尘浪、压力扭曲）
Signature：ManePressureWave（鬃毛压力波）
Impact：WideShockwave（大范围震荡）
Dissipation：HeavyDustFade（重尘沉降）
主要原语：Area、Trail、Instant、Composite

## 5. 技能 VFX 生命周期

统一设计阶段：
1. Prepare（准备）
2. CastStart（开始施法）
3. Charging（蓄力）
4. Release（释放）
5. Travel（飞行/传播）
6. Warning（区域预警）
7. Active（生效）
8. Impact（命中）
9. Sustain（持续）
10. End（结束）
11. Cancel（中断）
12. Fade（消散）

不是所有技能都必须包含全部阶段。

## 6. MobaPresentation（MOBA表现语义）映射

竞技玩法优先复用已存在的中立语义：
Moba.Ability.Cast.Start、Moba.Ability.Cast.Release、Moba.Ability.Projectile.Spawn、Moba.Ability.Area.Warning、Moba.Combat.Hit、Moba.Combat.Critical、Moba.Combat.Heal、Moba.Combat.Shield.Hit、Moba.Combat.Control.Apply、Moba.Status.Apply/Remove、Moba.Character.Death/Respawn。

项目公共表现模块不直接依赖 MobaPresentation；竞技组合根负责把竞技事实和项目上下文接入平台表现路由。

## 7. GamePlatformVFX（游戏平台VFX）原语映射

平台保持十种通用 Definition（定义）行为：Instant、Attached、Projectile、Beam、Area、Shield、Portal、Trail、World、Composite。

禁止增加 RatVFX / DragonVFX / TigerImpactVFX 等项目专属平台类。复杂终极技能优先使用 CompositeDefinition（复合定义）编排多个原语，而不是制造一个超大 Niagara System（粒子系统）承担所有生命周期。

## 8. Catalog / Definition 命名

Profile：Presentation.VFX.Hero.Zodiac.<Hero>.Default
Definition：Presentation.DBA.Hero.<Hero>.<Ability>.<Phase>
Catalog Entry：DBA.Presentation.Hero.<Hero>.<Ability>.<Phase>.<Variant>

当前真实技能尚未交付，上述 Ability 占位符只说明命名格式，不代表已存在生产 AbilityId。

## 9. Skin（皮肤）覆盖

HeroId + AbilityId + SkinId + Semantic → 项目 Catalog → 不同 Definition / Asset → 同一个 GamePlatformVFX 执行器。

皮肤不得改变 Gameplay AbilityId，不复制执行器，也不得成为项目核心硬依赖。

## 10. 内容包目录模板

每个 DBAHeroPack_* 在真实资产交付时建议：
Content/VFX/Common、BasicAttack、Passive、Abilities、Ultimate、Status、Movement、Definitions、Catalogs，以及 Content/Niagara、Materials、Textures、Meshes、Decals。

目录是规范，不允许为了完整度提前生成空资产或伪 .uasset。

## 11. 竞技可读性

- Warning（预警）与 Active（生效）必须明显区分。
- 敌方/友方识别不得只依赖细小颜色差异。
- 圆形、扇形、直线、环形区域由真实 Gameplay 参数驱动。
- 终极技能不能遮挡危险边界、角色轮廓或 HUD 关键信息。
- 团战优先保留本地玩家、当前目标、危险预警和关键终极技能，次要装饰效果允许降级。

## 12. 性能与生命周期

### 12.1 事件驱动
禁止项目层 Tick 扫描角色状态并重复决定是否播放 VFX。

### 12.2 预加载
角色选择确认后预加载本地英雄核心 VFX；MainArena 分配后按双方真实 Hero/Loadout 预加载高频战斗 Definition；世界切换释放旧 WorldGeneration 租约。

### 12.3 Pool（池化）
高频命中、拖尾宿主和短生命周期装饰优先池化，不把权威 Gameplay Actor 放入 VFX 池。

### 12.4 Effect LOD（特效质量分级）
按 Hero（本地玩家/当前目标/其他）、Distance（近/中/远）、Quality（低/中/高/极高）、Importance（关键/高/普通/装饰）综合降级。优先削减装饰粒子、次级光源、次级 Ribbon（带状轨迹）和远距离网格；区域预警边界和关键命中反馈不得首先移除。

### 12.5 Dedicated Server
不启用十二生肖纯表现内容包，不 Cook Niagara、纯表现材质、特效纹理和纯表现 Mesh；权威技能数据与 VFX Definition 分离。

## 13. 当前阶段落地边界

当前仓库真实 Ability 资产尚未交付，因此本轮要求：
1. 十二生肖 Hero VFX Profile 代码契约。
2. 12 个 HeroDefinitionId 的完整 Profile Catalog。
3. 项目表现自动化测试。
4. 内容包与命名规范。
5. 三层边界与性能规范。

后续每个真实 Ability Definition（技能定义）进入工程时，再按本文生命周期建立 Catalog/Definition/Niagara 资产，不提前虚构技能资源。
