# ZodiacReuseMatrix（十二生肖 VFX Lib 复用矩阵）

版本：1.0｜2026-09-30

本表只描述视觉复用方向，不定义生产 AbilityId（技能编号），不改变 Gameplay 权威规则。

## 1. 十二生肖复用矩阵

| 生肖 | 既定 VFX DNA | 可复用平台母版 | VFX Lib 原型来源 | 项目层改造重点 | 优先级 |
| --- | --- | --- | --- | --- | --- |
| 子鼠·影牙 Rat | 尖刃、突进、残影 | RibbonTrail、ProjectileSpiral、RadialBurst | ProjectileTrail / MagicImpact | 暗影银、牙刃切痕、快速收束 | P1 |
| 丑牛·玄角 Ox | 牛角、重脉冲、地裂 | GroundShockRing、DebrisBurst、ExpandingRing | FrostNova / GroundAOE / Impact | 冰晶替换为岩屑/尘土，强化地裂 | P0 |
| 寅虎·白君 Tiger | 爪痕、扑击、爆发 | RibbonTrail、RadialBurst | ProjectileTrail / MagicImpact | 白金能量、虎纹切痕、三/五爪结构 | P1 |
| 卯兔·玉灵 Rabbit | 月弧、玉光、花瓣 | SoftFloat、Orbit、GroundHalo、ExpandingRing | PetalBloom Jade / PetalsAura | 花瓣改玉屑/月瓣，保留轻盈上浮 | P0 |
| 辰龙·苍龙 Dragon | 螺旋、云气、升腾、雷 | ProjectileSpiral、Orbit、Beam、Mist | EnergyOrb / MagicBeam / Lightning | 云气替代硬拖尾，完整龙形只给关键演出 | P1 |
| 巳蛇·幽鳞 Snake | S 曲线、盘绕、追踪 | ProjectileSpiral、RibbonTrail、Beam | MagicProjectile / MagicBeam | 虹彩鳞光、蛇行追踪、盘绕命中 | P1 |
| 午马·雷蹄 Horse | 冲锋、雷叉、雷环 | RibbonTrail、ExpandingRing、GroundShockRing | Lightning / ChainLightning / FrostNova Ring | 雷蹄印记、接地电弧、限制高频重复 | P0 |
| 未羊·玉角 Goat | 玉角、穹顶、守护环 | GroundHalo、Orbit、Shield、SoftFloat | MagicShield / PetalBloom Jade | 玉盾、护盾波纹、玉屑消散 | P0 |
| 申猴·灵猴 Monkey | 棍弧、多段突进、烟爆 | RibbonTrail、RadialBurst、SoftFloat | MagicSpark / Trail / Impact | 金色笔触、烟雾弹、分段命中反馈 | P1 |
| 酉鸡·金鸣 Rooster | 扇形、金羽、音波环 | SoftFloat、Orbit、ExpandingRing | PetalBloom / PetalsAura / Nova Ring | 花瓣替换为金羽，音波边界保持清晰 | P0 |
| 戌狗·天犬 Dog | 追踪标记、扑跃、守护 | ProjectileSpiral、RibbonTrail、Orbit | EnergyOrb / Projectile / Trail | 灵体光、追踪印记、扑击短爆发 | P1 |
| 亥猪·玄鬃 Boar | 冲撞、压力环、尘浪 | GroundShockRing、ExpandingRing、DebrisBurst | FrostNova / GroundAOE / Impact | 去冰材质，改压力扭曲/尘浪/重碎片 | P0 |

## 2. 首批五角色建议

### 卯兔·玉灵 Rabbit
- GPVFX_SoftFloat：玉屑/月瓣漂浮。
- GPVFX_OrbitOffset：月环/玉光环绕。
- GPVFX_ExpandingRadius：柔和月环扩散。
- 通用 Halo/Mist/Glint SourceArt。
- 项目层候选资产：FXS_DBA_Rabbit_JadeMoon_Aura、FXS_DBA_Rabbit_JadePetal_Rise、FXS_DBA_Rabbit_MoonRing。

### 午马·雷蹄 Horse
- Ribbon Trail：高速冲锋轨迹。
- Expanding Ring：雷环。
- Ground Shock Ring：蹄击地面环。
- 第二批再补 Lightning / ChainLightning。
- 电弧频率必须受 Scalability 与距离控制。

### 未羊·玉角 Goat
- Ground Halo：守护光环。
- Orbit：玉屑环绕。
- SoftFloat：玉屑上升。
- 第二批再补 Shield（护盾技术母版）。
- Shield Ripple（护盾波纹）必须与真实护盾数值解耦。

### 酉鸡·金鸣 Rooster
- SoftFloat：羽毛漂浮。
- OrbitRise：羽光升起。
- ExpandingRing：音波环。
- Halo：晨光光晕。
- 复用 Petal 的运动，不复用花瓣造型；项目层使用金羽 Mesh/Texture。

### 亥猪·玄鬃 Boar
- GroundShockRing：重击地环。
- ExpandingRing：压力波。
- DebrisBurst：碎片喷射。
- 去除冰霜语义，改成 Dust / PressureDistortion（尘浪 / 压力扭曲）。

## 3. 内容包归属

真实技能 VFX 只允许放入对应 DBAHeroPack_<Hero>/Content 的 VFX/Common、BasicAttack、Passive、Abilities、Ultimate、Status、Movement、Definitions、Catalogs，以及 Niagara、Materials、Textures、Meshes、Decals 等目录。

当前首批五个 HeroPack 只有角色外观原型资产，尚未创建技能 VFX .uasset；在真实 Ability Definition 进入工程前不得制造空资产占位。

## 4. Definition 与 Catalog

项目 Definition 命名继续遵循：Presentation.DBA.Hero.<Hero>.<Ability>.<Phase>。
Catalog Entry：DBA.Presentation.Hero.<Hero>.<Ability>.<Phase>.<Variant>。
GamePlatformVFX 平台层不得出现 Hero / Ability 项目语义。

## 5. 复用优先级

P0：Rabbit、Horse、Goat、Rooster、Boar。
P1：Rat、Ox、Tiger、Dragon、Snake、Monkey、Dog。

这里的 P0/P1 是内容制作复用优先级，不是 Gameplay、网络或产品优先级。
