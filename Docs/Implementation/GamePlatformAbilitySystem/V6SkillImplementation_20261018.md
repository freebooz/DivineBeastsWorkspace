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
## 九、2026-10-18 V6继续执行：GAS P0 安全修复（未编译）

本批明确沿用用户先前的“不编译”约束：只修改当前 `DivineBeastsWorkspace` 里的项目源码、审核数据、非编译回归和文档；**未运行 UHT/UBT、MSVC、蓝图编译、Live Coding、Cook 或双客户端专服**。工程已有其他会话打开编辑器进行 VFX 相关审核，本批没有抢占或修改它的资源。

### 真实写入的修复

1. **冷却标签与效果一致**：`DivineBeastsAbilityLoadoutComponent.cpp`（项目技能授权组件）新增 `CooldownEffect->GetGrantedTags().HasAllExact(*CooldownTags)`（冷却效果必须真实授予GA所检查的所有精确标签）；V6进一步拒绝不属于其声明冷却集合的多余GrantedTag。调用依据为锁定UE5.8原生`UGameplayEffect::GetGrantedTags`，避免只有冷却时长而实际无法阻止再次激活。
2. **提交后目标位置再验证**：`DivineBeastsV6GameplayAbility.cpp`（V6原生执行器）新增 `IsV6WithinRange`（服务器实时施法距离），普通攻击在GAS Commit后重新射线命中同一个且相同化身代次的目标，不能使用提交前旧Hit直接结算。治疗/护盾/控制以服务器当前目标点生成CombatSpec，不重用已经迁移的旧冲击点。
3. **AOE范围中心冻结**：`BuildV6AreaTargets`（范围目标枚举）输出 `FrozenAreaCenter`（技能提交前冻结的权威中心），提交后重新确认中心仍在施法距离内、各目标仍在冻结半径内，避免同步Effect通知改变Pawn/目标位置导致旧AOE错误击中。范围多目标效果不是原子事务；先前已造成的真实伤害不伪造回滚，后继目标失效即停止。
4. **投射物冻结目标正确性**：显式记录 `bProjectileHadFrozenTarget`（提交前是否锁定），如锁定目标在提交后死亡或销毁，则拒绝悄悄降级为朝前直射；对仍合法的冻结目标重新核对距离、视线与化身代次并使用当前 `GetCombatTargetPoint`（战斗命中点）确定射向。没有锁定目标时才允许服务器前向直射，客户端视觉不能决定命中。
5. **短冲移动结果真实回报**：地面 `SafeDash`（安全短移）沿用导航落点与胶囊扫掠预检，提交后再次确认；如果最终SetActorLocation遇到动态阻挡却已实际走完部分距离，按实际移动成功的结果复制，不再表现为“人物移动了但技能完全失败”。用户未批准的历史穿墙/假身换位、S形毒路加速等仍不能凭此模式实现。

### 「蛇行」V6草案数值纠错

旧V2候选参数：蛇行位移`680cm`，射程却为`670cm`；V6执行器要求`CastRangeCm >= distanceCm`，导致基础模式无法通过。仅在 `V6SkillImplementationRegistry_20261018.json`（V6实施台账）新增 `v6DraftBalanceCorrections`（V6待审一级候选纠错），建议将V6作者稿的该技能射程调整为`680cm`，状态仍 `PENDING_DESIGN_REVIEW`（等待策划审查），`approvedForRuntime=false`（未批准上线）。`AuthorV6SkillDraftAssetsUE58.py`（V6原生草案作者脚本）制作**未批准DataTable**时才可消费该明确纠错，且验证身份、动作、原始数值、距离和审批标志。旧V2 JSON、旧已保存DataAsset及正式角色DefaultAbilitySetId均不改变。

注意：该修正仅解决原始距离不自洽，**不表示已实现历史「蛇行」的贴地S形毒径加速**，也不改变全部96技能和12被动未审核、未授予的事实。

### 本批实测的非编译门禁

- `Tests/Architecture/TestV6GASCombatSafety.py`（V6服务器战斗安全静态及故障注入）**11/11通过**（本轮追加AOE部分成功测试）。首次在未修复源码上出现基线失败，新增修复后全部通过；覆盖冷却GrantedTags、Commit后二次距离和射线、冻结AOE、真实弹道Aim和旧锁定目标失效、部分位移结果。
- `Tests/Architecture/TestV6GASImplementation.py`（V6 96技能台账/原生接线/候选修正）**13/13通过**。
- `Tests/Architecture/TestGASLoadoutTransactionSafety.py`（GAS授权事务回归）**20/20通过**。
- `Tests/Architecture/ValidateZodiacAbilityIntegration.py`（原生肖技能集成门禁）**52/52通过**。
- `git diff --check`（代码差异空白格式检查）退出码0。本批没有触碰其他会话VFX/地形的未提交资源。
- 本轮仍**没有**UE5.8新增C++编译、原生测试实跑、8槽Blueprint及96新GA/GE/DataAsset的冷重载、多人联机、Niagara人工验收或Cook证据。以上源代码修改只能标记“源码已改、静态验证已过”，不可升级为可上线。

### 仍需后续真正实施的能力与原生验收

- 12个控制槽、11个复杂终极、12项独立固有被动和其他历史复杂机制需要权威执行，而不是绑定到简单伤害/护盾。
- UE原生模拟 Commit 回调期间更换/删除目标、改变施法者位置、友军关系变化、同帧新障碍、动态AOE第17目标、投射物先后死亡与客户端去重。
- V6每技能真正的Cooldown GE/GAS成本与授权类、12英雄八槽AbilitySet、客户端8槽Widget、角色创建8技能图文与独立被动、96份48×48图标及已审Niagara资源制作均仍待原生完成。
## 十、继续实施批次：生肖控制技能复合结算源码（未编译）

本轮读取最新V6历史设计、96技能实施台账及平台Combat控制系统，按现行三层项目→平台单向依赖扩展`UDivineBeastsV6GameplayAbility`（项目V6唯一GAS基础技能执行器），未新增第二个ASC、战斗伤害计算器、Slow状态替身或VFX结算规则。仍不编译、Cook、改.uasset和提交主分支。

### 真实实现与边界

- 平台`EGamePlatformControlType`（通用战斗控制类型）目前只有`Stun`（眩晕）和`Silence`（沉默），**没有`Slow`（减速）**。因此新C++仅新增三种可表达V2单段两行为的执行模式：`DamageAndStun`（单体伤害后独立眩晕）、`DamageAndSilence`（单体伤害后独立沉默）、`RadialDamageAndStun`（服务器冻结范围中心的单段范围伤害后逐目标眩晕）。原来8种V6模式的枚举序列化顺序不变，新模式只追加在`SafeDash`之后。
- 数据合同严格要求动作数为2且顺序为`damage`（单段基础伤害）→`control`（单段控制），控制名称必须与技能原语精确一致，持续0～3秒、倍率0、命中段数1，必须与范围/目标模式、伤害表一致。不能用`stun`冒充四个`slow`控制槽。
- 按UE GAS原有`CommitAbility`（正式提交成本/冷却）后，再核验服务器Avatar代次、友军关系、目标距离/冻结范围、阻挡、有效命中点。实际伤害调用`UGamePlatformCombatComponent::ApplyDamage`（唯一平台权威伤害），成功才用**新`FGuid`**向同一目标调用`ApplyControl`（平台原生时效控制）。
- **两次真实GAS/Combat操作不是原子事务**。如伤害致死、技能在回调中被取消、目标更换化身或离开范围，跳过控制；若伤害已经成功，仍按真实伤害完成反馈，不能伪造全技能失败或倒扣生命。平台Combat独立发布伤害/控制事件。
- 有界AOE多目标处理改为统计`bAnyApplied`（是否至少命中任意合法目标）。某目标在Commit回调中失效时跳过该目标，不能否定之前已成功结算的其他目标；若来源Avatar失效立即停止迭代。
- 提供静态无世界合同`IsValidDamageControlAction`（原生控制行为字段校验），并新增`DivineBeasts.Abilities.V6.DamageControlPairContract`（引擎原生正负例测试源码）。UE5.8编译后须运行此测试，当前仅能确认源码存在。

### 12生肖控制槽逐项判断

| 历史生肖控制技能 | V2控制行为 | 新C++可能适配的基础原语 | 与V6历史效果的剩余差距 |
|---|---|---|---|
| 子鼠「咬筋」 | 伤害+减速 | 不支持 | 减速及位移抑制 |
| 丑牛「顶回」 | 范围伤害+眩晕 | `RadialDamageAndStun` | 必须实现定向顶退及冲锋打断 |
| 寅虎「虎啸」 | 范围伤害+眩晕 | `RadialDamageAndStun` | 历史为扇形减速、孤立条件短控，不应无条件群晕 |
| 卯兔「月痕」 | 伤害+减速 | 不支持 | 层数减速和条件定身 |
| 辰龙「龙压」 | 范围伤害+眩晕 | `RadialDamageAndStun` | 必须实现外推内牵的服务器位移规则 |
| 巳蛇「缠纹」 | 伤害+沉默 | `DamageAndSilence` | 历史为蛇纹减速及条件缠足，不能以无条件沉默冒充 |
| 午马「蹄震」 | 伤害+眩晕 | `DamageAndStun` | 冲势条件控制和额外击退未实现 |
| 未羊「安魂」 | 伤害+减速 | 不支持 | 守护型软控和减伤 |
| 申猴「扰心」 | 伤害+眩晕 | `DamageAndStun` | 需要烟弹扰乱锁定及假身条件效果 |
| 酉鸡「鸣震」 | 伤害+沉默 | `DamageAndSilence` | 仅显形或隐藏目标沉默、假身破相 |
| 戌狗「拦门」 | 伤害+减速 | 不支持 | 门盾阻挡与打断冲锋，不是普通减速 |
| 亥猪「震鬃」 | 范围伤害+眩晕 | `RadialDamageAndStun` | 岩甲在场才可短击退、否则近距减速 |

`Docs/Implementation/GamePlatformAbilitySystem/V6ControlSkillImplementationMatrix_20261018.json`（控制技能实施与人工审核矩阵）登记12个稳定ID、设计动作、候选模式、明确缺口及不授予门禁，分类为`4范围眩晕 + 2单体眩晕 + 2单体沉默 + 4未支持减速`。**8只是可复用基础原语候选，不表示8项正式V6历史控制技能完成，更不代表已生成8份GA蓝图。**

### 非编译门禁与下阶段交付

- `Tests/Architecture/TestV6DamageControlComposite.py`（复合结算源码与数据审核静态回归），包含控制种类、二段独立事件、同步回调重核、负例注入、历史技能不误批准以及现有枚举顺序检查。
- `Tests/Architecture/TestV6GASCombatSafety.py`（V6战斗安全回归）已追加AOE部分成功不能伪造整体失败的验证。
- `DivineBeastsV6AbilityContractTests.cpp`（UE5.8原生测试源码）追加纯动作正负例；未编译前不能声明原生通过。
- 下一阶段须实现服务器权威`Slow`属性、减速叠加和网络移动一致性，再落地历史条件控制、击退/牵引、假身/目标标记与12项独立被动，并补齐96项DataAsset/GA/GE、8槽Widget、角色创建图文/VFX与联机Cook证据。
- 当前控制源码回归 **13/13通过**、V6战斗安全源码回归 **11/11通过**，均仅为非编译静态检测与故障注入；并非引擎内原生自动化的执行结果。
## 十一、2026-10-18 平台原生GAS减速与V6四生肖控制扩展（源码阶段，未编译）

本批按用户继续执行要求补齐真正的Slow减速。代码落地于GamePlatformCombat（通用平台战斗层）与项目DivineBeastsCharactersRuntime（SSLM角色运动适配层）、DivineBeastsAbilitiesRuntime（V6 GAS技能执行器），**没有**新增第二套战斗引擎、复制RPC或Niagara判定。用户本轮要求修改最终提交`main`，同时保留“不编译、不Cook、不切换旧占位技能集”的约束。

### GAS服务器权威数值与状态

- 平台`UGamePlatformCombatAttributeSet::MoveSpeedMultiplier`（统一移动速度倍率属性）默认1，限时GE按加法聚合负减速比例；倍率经`PreAttributeChange`限制在0.30～1.00，最多减速70%，当前不提供额外加速。
- `UGamePlatformSlowGameplayEffect`（统一限时减速GE）使用`EGameplayEffectStackingType::None`（各来源独立有效句柄），`SetByCaller`（服务器动态设置比例）和`Combat.Control.Slow`（减速状态标签）。过期/驱散后由GAS属性重新计算，不维护第二份减速计时器、不使用Tick。
- 平台`UGamePlatformCombatComponent::ApplySlow`（服务器减速操作）严格验证目标ACharacter、真实CharacterMovement组件、属性集、源和目标Avatar代次、施法合法性、比例上限、持续秒数，GE应用同步回调后重新确认组件/世界/化身身份。相同事件ID在应用中被`InFlightSlowEventIds`（施加中事件集合）拒绝重复写入。目标换代时只撤销旧句柄，不清理后继；结束及重生清理过期和在途状态。
- `ResetForNewAvatar`（新化身重置）及`EnterDeadState`（死亡处理）均移除`Combat.Control.Slow`（减速状态），`RemoveControl(Slow)`（移除减速）通过已有GAS效果Tag撤销全部相关有限时状态，不直接修改属性Base。
- 角色`UDivineBeastsCharacterComponent`（生肖角色身份配置）继续以`HeroDefinition.Movement.MaxWalkSpeed`（英雄定义基础移动速度）为唯一基础值；监听GAS属性变更并将倍率施加于`CharacterMovement.MaxWalkSpeed`及`MaxWalkSpeedCrouched`（行走和蹲行速度），向客户端复制倍率以保持移动预测依据一致。切换ASC与退出时取消订阅，Ready恢复后重新计算，不每帧轮询；平台并不反向引用项目角色类。
- 当前通用减速属性按Actor相关性复制，**尚未**完成UE5.8客户端本地预测、网络平滑、迟到驱散及怪物AI行为实测，不能仅据静态检查宣称联机稳定。

### V6新模式及12控制槽对应

`UDivineBeastsV6GameplayAbility`（项目GAS技能执行基类）在既有`DamageAndStun`（伤害眩晕）、`DamageAndSilence`（伤害沉默）、`RadialDamageAndStun`（范围伤害眩晕）后追加`DamageAndSlow`（单体伤害后减速），避免改变旧枚举编号。第二动作必须为`control`且`ModifierId=slow`，倍率大于0小于等于0.70、持续大于0小于等于3秒、只有1个控制段。确认伤害生效后，再用另一服务器事件ID调用平台`ApplySlow`；伤害后的死亡、目标距离或身份变化都会取消减速阶段，但不回滚已成功伤害。

| 控制技能原语来源 | 数量 | 项目实例 | 当前状态 |
|---|---:|---|---|
| 单体伤害＋Slow | 4 | 子鼠咬筋、卯兔月痕、未羊安魂、戌狗拦门 | GAS源码候选，未编译 |
| 范围伤害＋Stun | 4 | 丑牛顶回、寅虎虎啸、辰龙龙压、亥猪震鬃 | GAS源码候选，未编译 |
| 单体伤害＋Stun | 2 | 午马蹄震、申猴扰心 | GAS源码候选，未编译 |
| 单体伤害＋Silence | 2 | 巳蛇缠纹、酉鸡鸣震 | GAS源码候选，未编译 |

**所有12项仅具备V2候选效果的基础服务端组合代码，并非V6历史技能完成。** 子鼠位移抑制、卯兔层数定身、未羊减伤、戌狗门盾拦截、龙压风环推拉、虎啸条件性群控、巳蛇蛇纹束缚与酉鸡揭隐等仍需独立权威行为。12份GA/GE正式蓝图、Definition、SkillSet、动画/VFX与创建角色界面未制作，`approvedV6ControlSkills=0`和96项`canGrantToServer=false`不变。

### 验证与交付边界

- 新`Tests/Architecture/TestV6SlowIntegration.py`（Slow静态合同及故障验证），检查平台GAS速度属性、GE标签、限时加法聚合、死亡重生清理、同步事件去重、双端基础速度与V6状态。
- 平台`GamePlatformCombatAttributeTests.cpp`（属性限幅原生测试）、`GamePlatformCombatEffectDefinitionTests.cpp`（减速GE结构原生测试）和项目`DivineBeastsV6AbilityContractTests.cpp`（V6减速动作测试）均已写入，**未编译或在UE内执行**。
- 旧V5批准显示名、96技能稳定ID、现有占位授权集合和客户端资源均不修改。非编译回归：V6SlowIntegration（减速上下游静态门禁）8/8、V6DamageControlComposite（控制组合）13/13、V6GASCombatSafety（战斗安全）11/11、V6GASImplementation（技能台账）13/13、GAS事务20/20、八槽数据15/15、V5名称9/9、生肖架构静态断言52/52，均通过；不将这些源码门禁等同UE内运行。
- 合并审核范围：中文注释覆盖GE持续效果、属性复制/限幅、ServerOnly应用与重入去重、角色移动及蹲行恢复、死亡换代清理；未审核或尚未实现的历史技能额外效果仍明确禁授，且没有新增客户端任意命令或第二套战斗结算系统。

- 默认全部V6`canGrantToServer=false`、`nativeAssetCount=0`，任何模式、矩阵和静态测试变化均不得自动解除审批禁令。

## 十二、V6平台强制位移原语与生肖推拉候选实施（源代码阶段）

**实施范围：** 在现有GamePlatformCombat（通用战斗模块）提供仅服务器能执行的`ApplyForcedDisplacement`（强制位移操作），SSLM项目层`UDivineBeastsV6GameplayAbility`（V6能力执行类）追加三个独立数据驱动模式。保持已批准V5显示名、96个稳定技能ID、旧默认占位技能集合不变。本批按先前指令不编译、不Cook、不修改.uasset；根据用户新指令，仅提交本任务文件到`main`，其他会话未提交文件不纳入。

### 真实权威行为与安全边界

1. `BuildDisplacementVelocity`（服务器推拉XY向量构建）仅接受`PushAway`（推离中心）或`PullToward`（拉向中心）模式。拒绝中心距目标25厘米以内、垂直差超过160厘米、水平速度低于100或高于800厘米/秒、未知模式及非有限坐标；不允许在上一次合法速度值残留时错误继续。
2. `ApplyForcedDisplacement`（服务器权威排队接口）使用已有`FGamePlatformCombatSpec`（战斗规格）及其唯一事件GUID、来源/目标Avatar代次、源目标世界与生存资格；禁止自身为目标、远程跨世界施加、无注册`CharacterMovement`（角色移动组件）或非地面状态的目标，目标/施法者距离不超过1400厘米，受影响实体距权威中心不超过700厘米。
3. 近端按目标`UCapsuleComponent`（胶囊碰撞）做真实`SweepSingleByChannel`（扫掠）；发现阻挡立即拒绝。随后由服务器调用现有`CharacterMovement->Launch`（角色运动组件排队XY强制速度），后续每个物理子步继续负责碰撞与网络运动。**结果事件为`DisplacementQueued`（强制移动已排队），绝不当作完成预期位移距离或成功打断冲锋。**
4. `MarkEventCompleted`（同源事件幂等账本）保证同一GUID不能重复施加；移动仅发送服务端已确认排队的中立战斗事件。客户端不拥有方向、位移距离、击退速度的权威RPC，也没有独立的VFX碰撞判定。自动网络平滑、落点安全和多人冲锋对抗尚需UE运行证据。

### V6数据合同与实际适用范围

追加`DamageAndPush`（单体向前伤害＋定向推离）、`RadialDamageAndPush`（范围伤害＋区域向外推离）、`RadialDamageAndPull`（范围伤害＋区域向内牵引）。所有三种模式必须有两个严格动作，先单次服务器伤害再单次`displacement`（强制移动），第二动作`ModifierId=push/pull`（推/拉）、速度100～800厘米/秒、持续时间0、仅一段；`Balance.CastRangeCm`（技能射程）不能超过1400厘米。

第二阶段操作在`CommitAbility`（真正GAS消耗/冷却提交）和`ApplyDamage`（平台权威伤害）成功后，再确认目标化身、友军关系、距离、阻挡和区域中心；以独立事件GUID排队移动。若目标被本次攻击击杀、近端通道受阻或目标在回调中失效，则不进行强制移动，但保留已结算伤害。不能将两段非原子操作伪造回滚。

本批不授予生产技能：`Docs/Implementation/GamePlatformAbilitySystem/V6ControlSkillImplementationMatrix_20261018.json`（V6控制槽评审矩阵）增加四项**历史推拉候选**：
- 丑牛`顶回`：可复用`RadialDamageAndPush`但仍缺前向扇形、冲锋判定与打断。
- 辰龙`龙压`：单次原语不足以模拟内圈拉近、外圈推离同时存在，必须另外实现双环时序、空间分区及条件短麻痹。
- 午马`蹄震`：只有冲势达到已批准阈值时才可附带推离，需要服务器气势/冲势状态门禁。
- 亥猪`震鬃`：需核验岩甲在场及减速流程后才能推进，不可直接无条件推离。

以上都是低层原语适配候选，**完整V6历史技能批准数仍为0，96项正式GA／GE/Definition仍不存在**。项目层不可用单一基础推拉模式替代多阶段历史终极或防御技能。

### 测试、风险与回退

- `Tests/Architecture/TestV6ForcedDisplacement.py`（推拉权威源码合同及8+静态回归）：检查速度、两端代次、胶囊扫掠、事件ID、伤害前置、GA两动作与4项历史候选审批隔离。它不是UE原生运行结果。
- `GamePlatformCombatDisplacementTests.cpp`（平台纯向量正反向原生测试）及`DivineBeastsV6AbilityContractTests.cpp`（V6推拉动作测试）需后续编译并执行UE5.8原生测试，当前只有源码。
- 强制位移网络同步需要实测对象：移动组件地面/空中模式变化、墙边突进和角落阻挡、多来源重复GUID、2人及5v5网络移动、物理更新顺序、濒死后取消、背向控制与低FPS。UE结果未取得前保持禁止授予。
- 回退只需不签发新的V6 GA和AbilitySet；用户现有12个占位集合以及早期显示名均不变。
