# 十二生肖技能 VFX 三层架构执行计划

版本：1.0
日期：2026-09-30
适用工程：DivineBeastsWorkspace（神兽联盟工作空间）

## 1. 目标

把十二生肖技能 VFX（视觉特效）落到现行三层插件架构中，并形成可持续扩展的工程基线：

- GamePlatformVFX（游戏平台视觉特效）只负责通用执行能力，不认识生肖、技能名、皮肤或 MOBA 私有规则。
- MobaPresentation（MOBA 表现语义）只负责施法、释放、命中、暴击、治疗、控制、区域预警等中立表现语义，不直接播放 Niagara（粒子特效系统）。
- DivineBeasts（神兽联盟项目层）负责 HeroId（英雄编号）、AbilityId（技能编号）、SkinId（皮肤编号）、项目表现上下文、十二生肖 Hero VFX Profile（英雄视觉特效配置）、Catalog（表现目录）映射和内容包资产。
- Dedicated Server（专用服务器）不得依赖或 Cook（烘焙）纯客户端 VFX 美术资产。

## 2. 已确认现状

1. DivineBeastsPresentationRuntime 已有 HeroDefinitionId / AbilityId / SkinId 项目表现上下文。
2. MobaPresentation 已有 Cast.Start、Cast.Release、Projectile.Spawn、Area.Warning、Hit、Critical、Heal、Shield.Hit、Control.Apply、Status.Apply/Remove、Death/Respawn 等中立表现语义。
3. GamePlatformVFX 已有 Instant、Attached、Projectile、Beam、Area、Shield、Portal、Trail、World、Composite 十种通用行为原语。
4. 十二个 DBAHeroPack_* 内容包已登记。
5. 当前真实 Ability 资产仍未交付，因此禁止虚构生产 AbilityId；本轮先落地英雄 VFX 配置契约和技能接入规范。

## 3. 执行步骤

### P0：保护现有工作树
- 保留当前 GamePlatformVFX 未提交修改。
- 不重建工程、不批量替换现有插件、不恢复已退休插件。
- 新增代码限制在 DBAClient / DivineBeastsPresentationRuntime 的项目表现职责内。

### P1：十二生肖 Hero VFX Profile 契约
新增 FDivineBeastsHeroVFXProfile 与 FDivineBeastsHeroVFXProfileCatalog。
每个核心英雄记录 HeroDefinitionId、ProfileId、ShapeLanguageId、MotionLanguageId、EnergyLanguageId、MaterialLanguageId、SignatureMotifId、ImpactLanguageId、DissipationLanguageId。
英雄编号必须从现有 FDivineBeastsProjectCatalog 唯一真源匹配，不建立第二套 HeroId 真源。

### P2：技能 VFX 生命周期规范
统一生命周期：
Prepare → CastStart → Charging → Release → Travel → Warning → Active → Impact → Sustain → End → Cancel → Fade。
该生命周期是设计/映射规范，不替代权威 Gameplay（游戏逻辑）生命周期。

### P3：三层语义映射
Gameplay Fact（玩法事实） → MobaPresentation Semantic（MOBA中立表现语义） → DivineBeasts Project Context/Catalog（项目上下文/目录） → GamePlatformVFX Definition（平台VFX定义） → Niagara/Material/Decal/Mesh/Light。

禁止：
- MobaPresentation 直接引用 Niagara。
- GamePlatformVFX 出现 Rat/Ox/Tiger/Dragon 等生肖类型。
- 项目 VFX 决定伤害半径、命中、控制时长等权威数值。
- 同一玩法事实由 MOBA 层和项目层重复广播两份相同效果。

### P4：内容包与资产组织
十二个英雄包统一 VFX 目录模板：
VFX/Common、VFX/BasicAttack、VFX/Passive、VFX/Abilities、VFX/Ultimate、VFX/Status、VFX/Movement、VFX/Definitions、VFX/Catalogs、Niagara、Materials、Textures、Meshes、Decals。
实际 UE 二进制资源必须由 Unreal Editor（虚幻编辑器）生成，不创建伪 .uasset。

### P5：命名与性能规范
Hero VFX Profile：Presentation.VFX.Hero.Zodiac.<Hero>.Default
项目 Definition：Presentation.DBA.Hero.<Hero>.<Ability>.<Phase>
Catalog Entry：DBA.Presentation.Hero.<Hero>.<Ability>.<Phase>.<Variant>
皮肤覆盖只替换 Catalog/Definition/Asset，不改变 Gameplay AbilityId。

性能基线：
- 事件驱动，禁止业务 Tick 扫描。
- 高频瞬时效果优先池化。
- 按真实 Loadout（装载）预加载必要定义。
- 采用 Effect LOD（特效质量分级）。
- 屏幕同类效果有预算和降级策略。
- 退出 World/AvatarGeneration 时释放句柄与租约。
- Dedicated Server 不启用英雄纯表现内容包。

### P6：验证
- 增加 UE Automation Test（虚幻自动化测试）。
- 校验核心英雄覆盖完整、ProfileId 唯一、所有视觉语言字段有效。
- 运行架构门禁/项目头文件检查。
- 条件允许时编译 DivineBeastsPresentationRuntime 或完整 Editor Target。
- 记录外部阻断，不把上游/其他插件失败误报成本任务通过。

## 4. 验收标准

1. 平台层源码中不新增十二生肖/项目技能概念。
2. MOBA 层不新增具体生肖资源依赖。
3. 项目层存在可编译的十二生肖 Hero VFX Profile 数据契约。
4. 12 个生肖 HeroDefinitionId 均能匹配唯一默认 VFX Profile。
5. 文档给出每个生肖的视觉 DNA、生命周期、VFX 原语映射、内容包组织和性能规范。
6. 自动化测试覆盖目录完整性与唯一性。
7. 所有新增英文标识均配中文说明/注释。

## 5. 本轮执行结果

- P0—P5：已完成。未覆盖现有 GamePlatformVFX 未提交修改，未新增平台/MOBA生肖专属类型。
- P1：已新增 FDivineBeastsHeroVFXProfile（生肖英雄VFX配置）与 FDivineBeastsHeroVFXProfileCatalog（生肖英雄VFX配置目录）。HeroDefinitionId 继续读取 FDivineBeastsProjectCatalog，不复制第二套英雄编号真源。
- P2—P5：已更新十二生肖视觉 DNA、统一技能VFX生命周期、三层语义映射、Catalog/Definition命名、皮肤覆盖、内容包目录、竞技可读性、预加载/池化/LOD/Dedicated Server规范。
- P6 头文件检查：通过。ValidateProjectHeaders.ps1 检查 432 处自有头文件引用，0 缺失。
- P6 三层继承边界：通过。PublicHeaders=427、Types=891、Edges=141、Passed=True。
- P6 项目层模块编译：通过。UBT 使用 -Module=DivineBeastsPresentationRuntime，仅执行5个Action；DivineBeastsHeroVFXProfile.cpp、DivineBeastsPresentationRuntimeTests.cpp 与 Runtime 模块成功编译并链接。
- P6 十二生肖静态覆盖：通过。Rat/Ox/Tiger/Rabbit/Dragon/Snake/Horse/Goat/Monkey/Rooster/Dog/Boar 各1条元数据记录，无缺失。
- P6 层级泄漏检查：通过。在 GamePlatform 与 MobaCommon 中均未发现 FDivineBeastsHeroVFXProfile、Presentation.VFX.Hero.Zodiac 或 DBA.VFX 项目标识。
- 完整 ValidateDesignBaseline.ps1：未通过，但18项均为当前工作树既有的 DBAWorlds、DBAArena、GamePlatformVFX 跨插件依赖声明问题；继承边界子项仍为 Passed=True，本次新增文件未出现在失败清单中。
- UE Automation Test：测试代码已编译；两次 UnrealEditor-Cmd 启动均未进入 Automation 测试队列。第一次超时于启动/SDK流程，第二次停在 TurnkeySupport 初始化阶段后人工停止，因此不得宣称 HeroVFXProfiles 运行时测试已通过。
- 现有 DBAPluginConsolidation Pester 脚本受本机 Pester 3.4.0 与脚本自身编码/解析错误阻断，未执行到测试断言；该问题不属于本次修改范围。

## 6. 后续触发条件

当前真实 Ability（技能）资产仍为0。后续每个正式技能 Definition 进入工程时，再按本计划的 AbilityId + Phase + Semantic 规则创建项目 Catalog/Definition/Niagara 资产；不得提前虚构生产技能ID或伪 .uasset。
