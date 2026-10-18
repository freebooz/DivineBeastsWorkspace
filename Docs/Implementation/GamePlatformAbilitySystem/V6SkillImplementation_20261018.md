# SSLM V6 十二生肖 GAS 技能实施记录（2026-10-18）

**工程：** `DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（UE5.8）。
**任务：** 在历史传承V6中为12名生肖的96项八槽主动技能建设实际服务器技能代码，并列出原生资产、战斗、美术及验证阻断。
**结果状态：** `CODE_WRITTEN_NOT_COMPILED_NOT_SHIPPED`。本批遵守用户此前“不编译”要求：未执行UHT/UBT、Blueprint编译、Live Coding、Cook、游戏联机或主分支提交。现有96项V6技能仍`PENDING_HUMAN_APPROVAL`（待人工审核）。

## 一、架构及执行归属

```text
GamePlatformAbilitySystem（平台唯一ASC与GAS Commit/Cost/Cooldown）
    ↓
GamePlatformCombat（平台权威Damage/Healing/Shield/Control/Projectile）
    ↓
DivineBeastsAbilitiesRuntime（项目V6技能身份/模式/目标/场景资格）
    ↓
DBAClient/DivineBeastsUIClient与DivineBeastsPresentationClient（只读8槽显示、VFX与音效）
```

不建立第二个ASC、伤害计算器、资源结算系统或客户端伤害远程调用。所有正式GAS技能的Authority由服务端独占，客户端只请求已授权的输入并观察复制的GameplayEffect、Combat事实、Cast事实和Projectile事实。

新代码：
- `Public/Abilities/DivineBeastsV6GameplayAbility.h`（V6原生技能执行公共接口）
- `Private/Abilities/DivineBeastsV6GameplayAbility.cpp`（V6服务器技能原语及安全检查）
- `Public/Definitions/DivineBeastsAbilityDefinition.h`（V6定义新增`bV6NativeApproved`，默认false）
- `Private/Definitions/DivineBeastsAbilityDefinition.cpp`（禁止DraftOnly和V6Approved同时成立；已批准V6仍验证具体字段）
- `Private/Components/DivineBeastsAbilityLoadoutComponent.cpp`（授予之前验证真实V6技能类、编排数据、Cost GE、Cooldown GE、Hero/Avatar代次）
- `DivineBeastsAbilitiesRuntime.Build.cs`（声明新增NavigationSystem私有导航依赖）

上述C++编写完成，**没有执行UE编译或运行时验证**。项目旧占位技能继续按`dba.abilityset.*_placeholder@1`（占位能力集合身份）加载，未变更12位英雄主资产的`DefaultAbilitySetId`（默认授权技能集），也没有创建96个新GA蓝图。

## 二、原生服务器技能执行能力（源码已编写）

| 执行模式 | 模式说明 | 审批与安全前置 | 本批状态 |
|---|---|---|---|
| `ForwardDamage`（前向单体伤害） | 服务端双通道射线查首个合法敌方实体，平台Combat执行真实伤害 | 源/目标世界、友军关系、化身代次、射程、单段数据 | 代码已写、待编译 |
| `SingleProjectile`（单枚权威投射物） | 平台AGamePlatformCombatProjectile执行射程与碰撞、客户端观察投射物事实 | Motion合法且射程吻合；过期锁定目标拒绝；每枚唯一事件ID | 代码已写、待编译 |
| `RadialDamage`（有限范围伤害） | 服务端Pawn重叠查询，有限目标集合、逐目标视线校验后用Combat结算 | 半径不超过700厘米、至多16个合法敌方实体、必须有合法范围中心 | 代码已写、待编译 |
| `SingleHealing`（单体治疗） | 对自己或服务器已确认友方目标治疗，按目标MaxHealth比例 | 限制50%最大生命、失效或敌方选择不得偷偷自疗 | 代码已写、待编译 |
| `SingleShield`（限时护盾） | 复用平台`ApplyShield`真实GAS效果实例，护盾不新建AttributeSet | 容量不高于50%最大生命，持续时间最大10秒，合法友方 | 代码已写、待编译 |
| `SingleStun`（单体眩晕） | 复用平台`ApplyControl`可取消GameplayEffect | 仅支持真正Stun，至多3秒，必须真实目标 | 代码已写、待编译 |
| `SingleSilence`（单体沉默） | 复用平台`ApplyControl`可取消GameplayEffect | 仅支持真正Silence，至多3秒 | 代码已写、待编译 |
| `SafeDash`（安全地面短移） | 服务器验证导航落点、地形高差及全胶囊碰撞后移动并复制 | 限950厘米；不支持穿墙、假身换位、空中飞跃 | 代码已写、待编译 |
| `Unsupported`（未知或复杂模式） | 默认不授予/不激活 | 多段、复合效果、毒路、假身、隐身、反隐、致命守愿等必须另行实现 | 显式拒绝，非已实现 |

这些执行模式都是有边界的**公共技术原语**，不是96个历史技能逻辑的全部实现。例如，“千月回环”是多段闪击回返，不能选`RadialDamage`模式就谎称已实现；“不动玄门”需要服务器权威实体门盾；“万象戏台”需要独立真假分身与换位状态。

单体／范围目标在提交GAS成本与冷却前进行服务器预检；提交后的同步回调再次确认技能激活令牌、角色、世界、化身代次。任何目标消失、死亡或更换化身都应拒绝迟到作用。范围攻击的多个独立目标结算**不是原子事务**：若中途产生死亡／目标失效，已经由Combat真实结算的伤害不能伪造退款或回滚，后续目标停止执行。

当前公共原语的参数以`FDivineBeastsEightSlotDraftAction`（已存在的结构化技能数据）＋`FDivineBeastsAbilityBalanceRow`（平台数值表）＋`UDivineBeastsAbilityDefinition`（项目数据定义）读取，真正技能GA的`AbilityDefinitionId`必须对应合法的同英雄定义资产，实际等级与Gas授权一致。

## 三、逐技能V6执行状态与数值台账

`Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json`（V6实施状态台账）按12生肖各8技能逐项保留：
- 唯一`AbilityId`（不更改稳定技能ID）、V6历史名称、已批准V5现名与专属机制；
- 一级V2候选伤害、段数、冷却、气势、射程、AOE及原始动作；
- 技术原语候选模式，仅供后续专家判断是否能严格等价；
- 新技能Definition、GA、GE、VFX的真实资产路径（均未生成，因此全部为null）、人工审批、编译、联机、发布状态。

目前只是按**旧V2单行为数据**计算技术候选，结果为：`ForwardDamage` 12、`SingleProjectile` 4、`RadialDamage` 11、`SingleHealing` 4、`SingleShield` 13、`SafeDash` 8、`Unsupported` 44，合计96。眩晕/沉默的V2控制技能同时带伤害，因此不能不经修改就降级成纯控制原语；相应模式目前没有可直接匹配的旧单行为资产。

**上述52项仅为底层执行原语的候选匹配，不代表52项V6历史技能完成或获批。** 所有96项V6原生正式GA、动态效果GE、数据资产、场景图和客户端资源均待作者与人工审核；实际已授权数量仍为0。

## 四、项目技能授予及DataAsset安全门禁

新`bV6NativeApproved`（V6原生行为已批准）默认false，与旧`bEightSlotDraftOnly`（草案禁用）不能同时为true。服务器批准逻辑：

1. 沿用原有平台`AcquireDefinition`（获取已签发主资产租约），核验英雄身份和Avatar代次。
2. 对每条V6 GA的类继承、稳定逻辑ID、项目Definition/数值表和GAS Cost/Cooldown进行交叉验证。
3. 只有符合真实`UDivineBeastsV6GameplayAbility`的已批准DataAsset才能授予；旧Configured父类不可借同名V6资产跳过执行器。
4. 只有单一已支持原语且动作与数值严格一致时才允许执行；所有副作用、额外段数、未知标签、错误资源成本均拒绝。
5. V6数据源审核未通过时，不清除草案标记，不修改默认技能集合，不向正式角色发放空壳技能。

普通攻击造成服务器实际伤害后通过角色现有`AuthorityAwardMomentumForConfirmedBasicHit`（确认命中气势奖励）发放一次气势。法术仍不消耗气势，旧五行／王印／阵营及生肖关系规则一律不恢复。

## 五、十二生肖尚需真实实现的个性技能

| 英雄 | 必须在专属服务端行为中补足的历史特色 | 当前结论 |
|---|---|---|
| 子鼠·影牙灵君 | 嗅弱感知、低伏背袭、钻缝路径、藏尾避选、三次合法换位影袭 | 未实现完整特色 |
| 丑牛·镇山玄角 | 可破坏山门实体、前向护卫墙、角顶反冲与破甲 | 未实现完整特色 |
| 寅虎·啸岭白君 | 裂甲层数、单目标猎物锁定、扑杀多阶段、虎啸条件硬控 | 未实现完整特色 |
| 卯兔·踏月玉灵 | 完美闪避窗、残影回返、定向月痕叠层、连续回环闪击 | 未实现完整特色 |
| 辰龙·御霄苍龙 | 风压推拉、风云雷三阶段法域、龙域持续时空控制 | 未实现完整特色 |
| 巳蛇·幽鳞玄蛇 | 具有足迹记忆与成熟引爆的服务器毒径、叠毒／抑疗 | 未实现完整特色 |
| 午马·赤焰雷蹄 | 基于真实奔跑距离的冲势、长线冲锋击退、友军支援抵达盾 | 未实现完整特色 |
| 未羊·玉角灵铃 | 条件性守愿单次免死、增疗符、轻控净化、群体祈愿脉冲 | 未实现完整特色 |
| 申猴·灵猴戏圣 | 权威假身实体和替身换位、戏台阶段及假身反制 | 未实现完整特色 |
| 酉鸡·金翎晨鸣 | 侦测范围与隐身显形、揭露陷阱／假身、反偷袭护盾 | 未实现完整特色 |
| 戌狗·守阙天犬 | 一名友军誓约绑定、方向性天门拦截、护主关键伤害承担 | 未实现完整特色 |
| 亥猪·岩甲玄鬃 | 承伤与低血战意、三段可反制地裂推进、破盾反震 | 未实现完整特色 |

每项新增服务端状态必须有拥有者、生命周期、弱目标/头像代次、复制最小事实、取消、断线与跨世界清理。不得用Niagara特效碰撞替代权威逻辑。

## 六、VFX、音频、8技能栏和创建角色页面

- 服务器只提交不可变`AbilityId`、已确认Cast事实、Projectile事实、Combat反馈；由`DivineBeastsPresentationClient`（项目表现客户端）接`GamePlatformVFXClient`（平台特效客户端）、`GamePlatformSFXClient`（平台音效客户端）解析。
- 96项独立48×48技能图标及96套Cast/Release/Travel/Impact/Cancel表现需要分别按实际动作制作、逐项人工审核；生肖图标合集不能冒充96项已审核纹理。
- 战斗8槽已有`Primary/Slot1/Slot2/Slot4/Slot3/Slot5/Slot6/Slot7`（1—8键）的C++语义及UI投影，但原生Widget绑定与真实GA授权需Editor核验。
- 角色创建页应显示12历史全称、固有被动及8技能的详细机制、CD、候选数值、效果图。可选图片未批准时显示“效果图待审核”，禁止将整页概念图直接用于Niagara投射。
- 不将客户端图片、Niagara、UI、音效硬引用放入Server专用Cook；专用服务器只加载战斗数据、GA、GE与必要权威Actor。

## 七、版本化原生资产制作与切换顺序

**第一阶段：** 评审通过V6名称、机制及数值，形成可观察的96项详细数据资产；在用户允许编译的新批次加载C++新增字段，再经当前工程Monolith制作。
**第二阶段：** 制作每个英雄8项GA Blueprint（能力蓝图）、GE成本和冷却（按真实数值）、12份AbilitySet及独立被动。单一简化原语可复用`UDivineBeastsV6GameplayAbility`；复杂历史机制必须有专门服务器行为，不得强配原语。
**第三阶段：** 依次通过数据校验、UE增量编译、原生自动化、真实Village和MainArena双客户端联机、5v5压力、客户端/Server Cook、96项纹理与VFX人工视觉签收、8槽UI及创建角色详情验证。
**第四阶段：** 按英雄逐个切换`DefaultAbilitySetId`，保留旧占位能力集回退。原生资产未生成／数据未获批／未编译的情况下，**绝不执行切换**。

## 八、已执行与未执行的验证

本轮按用户此前明确“不编译”要求，执行可运行的纯Python源数据、差异与静态合同检查，另保存隔离测试源码。**任何静态测试通过均不能代替C++编译、引擎内运行、多人网络或Cook证据。**

当前工程有并发执行作业，未关闭现有UE编辑器或替换共享DLL；V6未创建真实96份.uasset、不批准96项机制、不提交生产默认英雄资产。检查实绩以本轮真正执行的工具退出码为准，不能以“预期测试通过”报告。

详细文档目录见`Docs/Implementation/GamePlatformAbilitySystem/V6SkillDocumentIndex_20261018.md`（V6 GAS 文档索引）。
