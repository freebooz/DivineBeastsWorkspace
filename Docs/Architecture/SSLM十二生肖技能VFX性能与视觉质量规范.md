# SSLM 十二生肖技能 VFX 性能与视觉质量核心规范

> V1.0｜2026-10-15｜UE5.8｜适用`DivineBeastsWorkspace`（SSLM工作空间）及MainArena 1v1—5v5。
>
> **规则性质：** 后续代码、Niagara资产、性能优化及人工验收必须遵守的设计门禁。下述新通道、Effect Type资产和候选数字不是已经实现、已压测或已批准的发行标准。本规范由根目录`AGENTS.md`引用；非竞技世界可复用机制，但不直接使用竞技专用预算。

## 1. 核心目标与公平边界

- **优先保留视觉质量和打击感：** 角色起手蓄力、技能主体剪影、释放/飞行方向、命中主闪光、冲击波及短时余烬有明确主次；在近景保证电影感，优化时先削减远景装饰、烟雾、重复火花和额外光源，避免所有粒子统一减半使技能“无感”。
- **关键游戏信息永不依赖可剔除装饰：** 敌我重要危险区域、方向、命中位置与控制提示必须在Low档和负载超标时仍准确、清晰；使用低成本Mesh（网格体）、Decal（贴花）、材质或HUD保底层，粒子只补充情绪表现。敌方威胁不能仅因阵营或距离被裁掉。
- **权威事实不变：** Dedicated Server（专用服务器）/GAS决定命中、伤害、冷却、范围、控制、比赛结果。Niagara GPU/CPU碰撞、Flipbook或动态地面火焰不得产生权威判定；客户端特效失败、预测取消和质量切换不能改变Gameplay。
- **目标基于真实测量：** System实例数关系CPU调度，Emitter/粒子关系模拟负担，材质透明Overdraw（重复绘制）、折射/SceneColor（场景颜色采样）及光源关系GPU成本；同时度量并平衡，不能只凭一个数保证帧率。正式帧率目标必须记录硬件、分辨率、RHI、阴影、TSR及打包客户端版本。

## 2. 唯一三层链路与资产归属

依赖方向保持`DivineBeasts项目层 → MobaCommon通用MOBA层 → GamePlatform平台层`。项目可消费平台公开中立接口，反向依赖禁止。

- `GamePlatformPresentation`（平台表现协调）负责中立表现事实链及Provider路由；`MobaPresentation`（MOBA表现）仅把`Cast.Start`（施法）、`Cast.Release`（释放）、`Projectile.Spawn`（弹道）、`Hit`（命中）、`Critical`（暴击）、`Area.Warning`（区域预警）、`Status.Apply/Remove`（状态起止）转为语义，不直接播放Niagara。
- `GamePlatformVFXClient`（平台客户端特效模块）是唯一VFX执行机制，复用已有`UGamePlatformVFXWorldSubsystem`（世界子系统）、`UGamePlatformVFXDefinition`（特效定义）、`GamePlatformData`（资产租约）、Scalability（伸缩策略）、Effect Type、Diagnostics（诊断）和Niagara原生Pooling（池化）；通用NDC共享服务将来只增量接入，不能新建第二套播放器/预算真源/资产管理器。`GamePlatformVFXEditor`（编辑器模块）统一负责真实资产检查。
- `DivineBeastsPresentationRuntime/Client`（SSLM项目表现）用已存在`FDivineBeastsHeroVFXProfile`（英雄视觉配置）、真实HeroDefinitionId/AbilityId/SkinId及唯一Catalog/Definition选择视觉特征、各阶段表现、资产与质量变体；不得为十二生肖各复制一套C++执行器或伪造生产技能ID。
- 公共火球/冰箭/奥术/燃烧及开发占位归`DBAPresentationPack_Core`（公共表现包），英雄专有资源归已有`DBAHeroPack_*`（生肖内容包），世界/竞技专有归实际登记的所属包；专用服务器不可依赖或Cook纯客户端Niagara/纹理/材质/人工审核地图。
- **唯一请求流程：** Gameplay事实→中立Presentation请求（RequestId/ActivationId/PredictionKey）→项目Catalog/Definition→平台VFX世界服务→独立Niagara执行**或**NDC中立共享模拟→客户端渲染；NDC必须位于原有请求去重、预算和生命周期校验之后。

## 3. 十二生肖技能制作合同

### 3.1 生命周期和必填台账

设计阶段统一阶段：`Prepare → CastStart → Charging → Release → Travel → Warning → Active → Impact → Sustain → End/Cancel → Fade`（准备、前摇、蓄力、释放、飞行、预警、生效、命中、持续、结束/取消、消散）。只配置技能真实发生的阶段，以权威技能时序触发，预测与确认不得重复播放。

每个技能及暂用占位必须登记：真实英雄与技能ID、HeroVFXProfile（英雄视觉配置）、中立阶段语义、DefinitionId、Niagara资产软引用、Effect Type、核心剪影和装饰层、System与Emitter数量、GPU/CPU模拟、Bounds、Flipbook/材质采样、质量变体、可剔除距离、资产预加载、取消/停止/归池、预计过绘成本、性能实测和人工视觉证据。禁止仅因制作规格存在便宣称真实`.uasset`已生成。

十二生肖采用Profile定义的形态、运动、能量、材质、标志元素、命中和消散语言，保障各英雄第一眼辨识度；公共火球等占位只覆盖视觉外观，未来替换真实技能时保持既有生命周期和请求身份。

### 3.2 推荐效果组合

| 生命周期/组件 | 推荐实现 | 优化方向 |
|---|---|---|
| 蓄力/前摇 | 1个可取消System＋低成本法阵Mesh/材质 | 多层符文合并，不独立Spawn多套 |
| 火球飞行主体 | 发光几何核心＋火焰Flipbook/程序化材质＋少量拖尾 | 独立1个生命周期System；不堆多层巨大Additive透明片 |
| 冰箭/奥术飞行 | 冰晶网格/奥术核心＋简洁动感尾迹 | 保留清晰轮廓和速度感，不用粒子判定伤害 |
| 高频瞬间命中 | 保底冲击点＋NDC共享GPU火花、冰晶、奥术破碎 | 重复事件不每次创建完整System |
| 爆炸/终极技能 | 受控主体System＋共享碎片/烟尘 | 限制复合子System，突出打击瞬间 |
| 持续燃烧/减益 | 单个附身/区域持续System或材质状态 | 持续伤害每跳不新建System |
| 地面AOE/危险提示 | 权威范围驱动Mesh/Decal/材质提示，Niagara作装饰 | Low档也保留准确半径 |

Flipbook（序列帧）是火焰、烟雾、爆炸的可选层，与Mesh/材质/Niagara混用；1024/2048纹理图集及16—64帧仅是评估起点，最终依据视觉清晰度、采样、压缩、显存和过绘选取。避免把4K/8K透明图集、折射及动态粒子光源当作普遍要求。

### 3.3 Emitter/模拟/材质

普通命中1—2、普通弹道2—3、火球/冰箭3—4、AOE爆发3—5、终极技能5—8个Emitter为**评审触发参考值**，非引擎硬限；超过需视觉依据和实测。相同生命周期、Bounds和剔除规则的粒子可合并Emitter；远距离多事件不能强制合并成覆盖整个地图的巨大System。

大量纯视觉火焰、烟、碎片优先GPU Simulation（GPU模拟）；仅CPU交互必须时用低数量CPU Simulation（CPU模拟）。GPU System使用实测准确的`Fixed Bounds`（固定边界），飞行弹道用跟随组件的紧致局部Bounds；按距离/视锥/离屏停更管理，不沿用未经5.8核验的弃用字段。以Shader Complexity（着色器复杂度）和Quad Overdraw（重复绘制）检查透明像素，减少SceneColor、折射、逐粒子动态光源和阴影。


### 3.4 十二生肖逐英雄优化映射（遵循既有Profile，不重新设计技能真源）

下表是将现有`Game/Plugins/DivineBeasts/DBAClient/Docs/ZodiacSkillVFXArchitecture.md`（十二生肖既定视觉DNA）和`DivineBeastsHeroVFXProfile.cpp`（现有项目视觉语言元数据）映射为**待制作/改造的特效优化策略**。不得把这些建议视为新AbilityId、现成Niagara资产、最终伤害类型或已通过的视觉验收。生肖视觉语言仍以已有Profile为唯一项目视觉标识真源。

| 英雄 | 应保留的标志视觉 | 推荐高并发实现 | 优先缩减的装饰成本 |
|---|---|---|---|
| 子鼠·影牙 | 弧刃、牙痕、折线突进、暗影银光 | 一段短Ribbon（带状轨迹）/Mesh残影＋NDC共享十字切割闪光 | 多段残影实体、长寿命暗雾 |
| 丑牛·玄角 | 宽重牛角、重压环、地面震裂 | 冲击环Mesh＋受限地裂材质，尘土/碎屑写入NDC地面通道 | 地裂连续Decal与大范围厚尘 |
| 寅虎·白君 | 白金虎爪、斜斩、虎啸爆发 | 爪形Mesh/简洁拖尾＋共享命中冲击环 | 层叠爪痕、连续大面积发光条纹 |
| 卯兔·玉灵 | 玉色月弧、轻盈环光、花瓣上升 | 月弧轮廓Mesh/材质＋少量GPU玉屑，护盾核心独立持有 | 大量半透明花瓣与重叠玉环 |
| 辰龙·苍龙 | 苍青云气、龙首云纹、螺旋升腾 | 普通技能只用局部龙首/龙爪/盘旋Ribbon；完整龙形仅限关键终极演出 | 多个巨型云雾System、持续全屏龙焰折射 |
| 巳蛇·幽鳞 | S形灵蛇曲线、鳞片幽光、盘绕命中 | 沿弹道的紧致Ribbon/Mesh＋材质鳞光，共享盘绕命中火花 | 重复跟踪子System、长时间半透明鳞片 |
| 午马·雷蹄 | 雷电脉络、蹄印、冲锋冲击环 | 一套主电弧尾迹＋限速地面蹄印事件；命中闪电环可用轻量Mesh | 每帧产生的新蹄印、粒子动态光源 |
| 未羊·玉角 | 玉角符纹、护盾穹顶、局部波纹 | 单一护罩Mesh/动态材质参数，命中局部波纹按事件更新 | 护盾每跳新System、大面积叠加透明球 |
| 申猴·灵猴 | 棍扫弧线、翻转残影、多段命中 | 棍弧Mesh/Ribbon＋有界少量残影，多段打击走NDC聚合短爆发 | 每一击独立烟雾爆炸和无限影分身 |
| 酉鸡·金鸣 | 扇形金羽、晨光脉冲、鸣叫环波 | 羽扇轮廓与音波环Mesh，少量GPU金羽采用共享突发 | 漫天羽毛长期存在、大面积加色覆盖 |
| 戌狗·天犬 | 追猎印记、守护环、扑击打点 | 一份可更新的追踪标记材质/附着System＋共享扑击命中 | 每次追踪更新重复Spawn、重复印记 |
| 亥猪·玄鬃 | 宽弧鬃毛、重压波、厚重冲击 | 压力环Mesh＋短时碎屑/尘土共享NDC，保留重击主闪光 | 长寿命厚尘、低档折射扭曲 |

**统一执行门槛：** 普通技能优先采用“1个受控核心System（可选）＋可共享的命中事件”，不把每个表现阶段机械拆成新System；重要终极技能允许更丰富的独立演出，但须提交10名英雄同时释放时的实例数、CPU/GPU/Overdraw实测与降级替代。对于护盾、区域预警和标记，即使粒子被裁剪也要保留真实形状、位置和时长。

## 4. MainArena的Effect Type与候选预算

候选值为**单客户端World的真实活跃Niagara System数量**，包括共享NDC监听、独立弹道、环境和绕过平台VFX播放路径的其他Niagara；NDC每次短时事件不是一个新的System。

| 通用Effect Type建议名称 | Epic | High | Medium | Low |
|---|---:|---:|---:|---:|
| `NET_GP_ArenaCritical`（关键战斗） | 20 | 16 | 12 | 10 |
| `NET_GP_CombatCast`（施法蓄力） | 18 | 12 | 10 | 6 |
| `NET_GP_CombatProjectile`（弹道） | 24 | 16 | 10 | 8 |
| `NET_GP_CombatImpact`（命中） | 20 | 12 | 8 | 4 |
| `NET_GP_CombatArea`（持续范围） | 16 | 10 | 8 | 4 |
| `NET_GP_CombatStatus`（状态） | 12 | 8 | 6 | 4 |
| `NET_GP_ArenaAmbient`（环境装饰） | 10 | 6 | 4 | 4 |
| **合计候选** | **120** | **80** | **58** | **40** |

`Max Effect Type Instances`（同类累计实例上限）与`Max System Instances`（同一Niagara System资产实例上限）分层生效；配置两者后仍需保证关键技能通过非Niagara保底。Distance Significance（距离重要性）在同类内排序，但不理解MOBA威胁，需业务重要度优先再由Niagara执行原生距离/数量伸缩。

现有平台`MaxActiveInstances=256`、`MaxStatusInstances=192`、`MaxAmbientInstances=96`、`HardMaxTrackedInstances=512`、`MaxPendingInstancePreloads=64`是平台**追踪请求**的容量控制，非GPU毫秒/真实System预算；Critical也受硬限。Arena候选预算必须作为按World、模式、设备、质量设置的增量策略，不能直接全局下调插件INI影响OpenWorld/Village。

| 初始质量目标（待P0测量批准） | Epic高端PC | High主流PC/主机 | Medium | Low |
|---|---:|---:|---:|---:|
| 目标FPS | 120 | 60 | 60 | 60 |
| 全帧时间 | 8.33ms | 16.67ms | 16.67ms | 16.67ms |
| VFX GPU预算 | ≤2.0ms | ≤1.5ms | ≤1.0ms | ≤0.8ms |
| VFX CPU预算 | ≤0.8ms | ≤0.6ms | ≤0.5ms | ≤0.4ms |
| 装饰粒子比例 | 100% | 75% | 50% | 25% |

CPU/GPU预算分别统计，不能相加直接当总帧时间。Emitter级`Spawn Count Scale`（生成数量缩放）只作用装饰层，不能让关键弹道、命中闪光或真实危险区域轮廓在Low档不可见。优先裁剪远距重复装饰，其次减少动态光、热浪/折射与高屏幕覆盖，最后降主体复杂度。

Budget Scaling（预算缩放）使用逐级、带恢复滞后的软降级；`Max Global Budget Usage`（全局FX预算比例）**不是**GPU使用率百分比。距离可先评估25m/50m/80m分带（引擎默认单位厘米），但须结合MainArena镜头、技能实际射程和预警需求，不统一隐藏远景敌方危险效果。

## 5. NDC共享高频效果与原生组件池

建议在平台通用层按需实现以下中立通道，**当前仅为目标方案**：
- `NDC_GP_CombatImpact`（命中火花/冰晶/短爆发）。
- `NDC_GP_GroundBurst`（地面尘土、碎屑、火星）。
- `NDC_GP_AmbientBurst`（短时环境粒子，已有鸟群/天气GPU共享机制不重复迁移）。

使用Niagara Data Channel Islands（通道空间岛屿）或有界空间区域，一个或少量共享System读入多次短时事件并生成粒子；避免“120次命中=120个独立System”，也避免一套巨大Bounds常驻全竞技场损失剔除。共享System仍占用GPU模拟、粒子、显存和渲染预算。

推荐在CPU侧记录`RequestId`（请求去重）、`ActivationId`（激活关联）、`PredictionKey`（预测）、`WorldGeneration`（世界代次）、`Location`（位置）、`Normal`（表面法线）、`StyleId`（中立视觉样式）、`SurfaceType`（地表类型）、`Importance`（重要性）、`Color`（颜色）、`Scale`（尺寸）及`Seed`（随机种子）；实际写入Niagara NDC的GPU载荷须以UE5.8支持字段类型为准，不盲目把复杂身份全塞入GPU数据通道。平台不能出现生肖名。

独立弹道、跟随角色的持续状态、可打断前摇、持续AOE和大招主体仍走既有`NiagaraExecutor`＋`ENCPoolMethod::AutoRelease`（原生池自动归还），不新建平行池。NDC事件写入必须遵循已有Presentation去重、Definition解析、World生命周期和取消/纠正身份；预测/确认只产生一次正确视觉爆点，旧世界的事件不能进入新竞技场。

高频事件按优先级、距离、屏幕位置和事件时效执行背压；核心危险提示与主要命中**不排队延后且不因可选装饰过载消失**，次要碎屑/烟雾可缩小、合并或舍弃。High先按150次/秒的短效果事件构建专项压力输入，真实吞吐阈值再由P0/P2测试定版。

## 6. 运行时、预加载与资源生命周期

- 进入MainArena前依据十名英雄真实Loadout（装载技能）确定需要的Profile/Definition/软引用资源，复用GamePlatformData共享租约和现有有界缓存；关键资源预加载、常用System适量预热，不在战斗第一发技能时大量同步加载或Shader编译。
- 在Spawn前检查类别预算、有效时间、距离和视锥；可选离屏效果不必创建后再销毁。持续状态按事件激活、参数更新与事件停止，不因DoT（持续伤害）每跳新建Niagara。
- 组件池设定最大占用、预热数量和回收验证，过度预热、闲置大池和超大缓存也是性能问题。Niagara异步回调/定时器/共享系统必须校验World代次及取消状态。
- 正常结束、打断、预测撤销、死亡、换角色、重连、EndPlay、Arena结算及World销毁按所有权停止实例、取消监听与定时器、撤销注册并释放自己持有的租约，不影响其他World或PIE消费者。
- 分辨游戏线程、渲染线程和GPU成本；避免每帧遍历全表寻找播放句柄，继续复用现有反向索引和一次性生命周期清理。

## 7. 开发顺序与发布门禁

**P0盘点与基线**：统计真实Niagara System、Emitter、Effect Type、材质Overdraw、GPU/CPU、十名英雄最坏5v5场景；不能以VFX追踪请求数量冒充全部Niagara。

**P1 Effect Type和Arena预算**：在现有GamePlatformVFX中增量加入中立分类/世界预算，默认、平台、四画质System必须与Definition的Effect Type、FixedBounds一致；不重构无关插件。

**P2 NDC**：建立中立高频命中/地面共享System，验证多空间分区、事件限流、去重、预测取消和World退场。

**P3 十二生肖资源**：完成公共火球/冰箭/奥术/燃烧的Mesh/Flipbook/材质及各阶段视觉，并按英雄Profile配置技能视觉DNA、四档质量变体与人工对照。

**P4 生命周期和性能**：分帧预载、池预热、Spawn预剔除、距离LOD、离屏处理、材质Overdraw整治，保证关键提示永不丢失。

**P5 自动门禁**：Editor资产Validator、配置与生命周期测试、NDC共享和重复请求专项回归、三层依赖/服务器隔离校验。

**P6 完整验证**：统一UE5.8 Editor/Client/Server构建、Niagara真实GPU编译、实际打包客户端压测、Client Cook和Dedicated Server不含纯VFX内容的产物审计。

**P7 人工签收**：插件演示场景和真实MainArena核验蓄力、释放、飞行、命中、持续、消散的神秘/自然/电影感、打击感和敌我可读性。

建议以`stat Niagara`（Niagara统计）、`stat GamePlatformVFX`（现有VFX计数）、`stat GPU`、`profilegpu`、Niagara Debugger、Unreal Insights、Shader Complexity/Quad Overdraw记录：真正活跃System总数、分类峰值、NDC共享实例与事件、Pool命中率、首次施法卡顿、透明过绘、GPU/CPU耗时、P95/P99帧时间、内存及10分钟稳定性。

**High初始可验证门禁：** P95帧时间≤16.67ms、P99≤20ms、VFX GPU≤1.5ms、CPU≤0.6ms；Epic 120 FPS以8.33ms/帧独立评估。必测所有1v1—5v5模式、近距离十人集中释放/多AOE/150次每秒短时命中压力、四档画质、预测/确认/纠正/取消、断线重连、退出再入无残留、GPU热路径、真实客户端与服务器Cook。机型、分辨率和图形设置不同的记录不混为同一结论。

**禁止发布的情况：** 危险提示缺失、权威范围错位、预测重复爆炸、无界实例/租约泄漏、低画质基本不可辨认、服务器打入纯VFX、未经过独立GPU压测和人工视觉验收却标注Production Ready（生产就绪）。

## 8. 当前依据（2026-10-15）

只读对照`GamePlatformVFXSettings.h`（平台预算）、`GamePlatformVFXScalabilityPolicy.cpp`（实例软硬保护）、`GamePlatformVFXNiagaraExecutor.cpp`（原生Spawn与池）、`GamePlatformVFXDefinition.h`（Effect Type/QualityVariants/FixedBounds）、`GamePlatformVFXDefinitionValidator.cpp`（系统一致性验证）、`Docs/superpowers/plans/2026-09-30-zodiac-skill-vfx.md`（既有技能三层生命周期）。源码搜索尚未发现通用NDC实现；不能据此声称已盘点完UE二进制内容资产。本轮仅制定规范，不宣称编译、UE生成资产或性能实测通过。

UE5.8官方技术参考：
- [Performance Budgeting Using Effect Types（Effect Type预算）](https://dev.epicgames.com/documentation/en-us/unreal-engine/performance-budgeting-using-effect-types-in-niagara-for-unreal-engine)
- [Scalability and Best Practices（Niagara优化）](https://dev.epicgames.com/documentation/unreal-engine/scalability-and-best-practices-for-niagara)
- [Niagara Data Channels Overview（NDC共享模拟）](https://dev.epicgames.com/documentation/unreal-engine/niagara-data-channels-overview)
- [FNiagaraSystemScalabilitySettings（系统伸缩配置）](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/Niagara/FNiagaraSystemScalabilitySetting-)
