# DBAGameplay：十二生肖技能数据驱动与权威授权

> 适用工程：DivineBeastsWorkspace（神兽联盟工作空间）。
> 当前实施状态：已写入 C++ 源码及部分装配，**未通过锁定 UE5.8 引擎编译与真实资产/联机验收**。
> 依据：根 AGENTS.md（工程规则）、Game/Plugins/插件开发规范.md（插件规范）。

## 模块归属和三层边界

- GamePlatformAbilitySystem（平台 GAS 技能系统）保留原生 ASC、AbilitySet 和 FGamePlatformAbilityGrant（单项技能授权）稳定契约；GamePlatformCombat（平台战斗）保留伤害计算与命中验证；GamePlatformData（平台数据）负责逻辑主资产 ID 和生命周期租约。
- MobaCommon（MOBA 可选中间层）不新增生肖技能类；竞技特有效果只在 MobaPresentation（MOBA 表现）把竞技事实映射成中立语义。
- DBAGameplay（神兽联盟项目玩法插件）新增 **DivineBeastsAbilitiesRuntime（技能运行模块）**，其中 UDivineBeastsAbilityDefinition（技能逻辑资产）与 FDivineBeastsAbilityBalanceRow（每级数值行）负责玩法数据；UDivineBeastsAbilityLoadoutComponent（角色技能装配组件）根据服务器已认可的 HeroDefinition（英雄定义）中的 DefaultAbilitySetId（默认技能集编号）加载现有 AbilitySet 并授权技能。
- DivineBeastsCharactersRuntime（角色身份模块）只新增 FName DefaultAbilitySetId（稳定逻辑编号），不引用具体技能类、逻辑定义、UI 或图标；新运行模块依赖角色模块，不反向依赖，插件关系保持单向。
- DBAServer（服务器控制面）本轮没有新增全球单例技能管理器；项目实例出生/准入需要在 OpenWorld 与 Village 的真实 Pawn 组合点采用 ADivineBeastsGameplayCharacter（可玩角色）或显式安装同等三组件。当前只将竞技场服务器的出生原型改为此类，其他角色还不能宣告打通。

## 真实公开类型（新增源文件，待 UE 编译）

- Public/Types/DivineBeastsAbilityBalanceRow.h（技能数值表字段和单位）。
- Public/Definitions/DivineBeastsAbilityDefinition.h（项目技能定义；继承 GamePlatformDefinitionBase，不复制 LogicalId）。
- Public/Abilities/DivineBeastsConfiguredGameplayAbility.h（数据驱动技能基类：只读预加载数值，服务器调用 GamePlatformCombat 伤害接口）。
- Public/Components/DivineBeastsAbilityLoadoutComponent.h（OwnerOnly 技能授权事实复制；带当前 Hero ID、AvatarGeneration 和 Revision）。
- Public/Characters/DivineBeastsGameplayCharacter.h（单一 ASC、角色身份组件、技能装配组件与 GamePlatformCombat 战斗组件）。
- Private/Tests/DivineBeastsAbilityDefinitionTests.cpp（技能数值字段校验）、Private/Tests/DivineBeastsAbilityAssemblyTests.cpp（反射和默认拒绝状态）。

## 数据配置合同与单一真源

HeroDefinition.DefinitionId（英雄定义编号） → HeroDefinition.DefaultAbilitySetId（默认技能集合逻辑编号） → GamePlatformDefinition:AbilitySet（平台主资产） → AbilityGrant.AbilityId（规范技能身份）、AbilityGrant.AbilityClass（GAS 技能类）、AbilityGrant.InputTag（输入标签） → AbilityClass.AbilityDefinitionId（项目玩法定义主资产） → AbilityDefinition.BalanceTable 与 LevelRowNames（权威每级数值表）。

1. DefaultAbilitySetId 为空时保留旧英雄资产的读取兼容性，但不给出任何技能，明确报告未配置。不能为十二生肖制造未经批准的正式 AbilityId、技能名或伤害数字。
2. FGamePlatformId（平台逻辑身份）规范形如 namespace.name@version（命名空间.名称@版本），并非 .uasset 文件路径。新 UDivineBeastsAbilityDefinition 继承 LogicalId，不再定义第二个 AbilityId；Balance 行与技能授权使用该规范字符串互相校验。
3. 技能数值表字段：Level（等级）、BaseDamage（基础伤害）、AttackPowerCoefficient（攻击加成）、AbilityPowerCoefficient（技能强度加成）、DamageType（平台通用伤害类型）、bCanCritical（是否允许暴击）、CooldownSeconds（冷却秒数）、MomentumCost（气势消耗）、CastRangeCm（厘米施法范围）、AreaRadiusCm（厘米范围半径）。输入参数/单位/合法区间由 Row.Validate 校验。
4. TryGetLoadedBalance（已加载数值查询）禁止 LoadSynchronous（同步加载）；正式资产须在 AbilitySet 的 RequiredDefinitions（定义依赖）列出对应玩法定义，平台 AcquireDefinition 同时加载 AbilitySet 与 Gameplay 分组，缺失则拒绝伤害与施法。
5. GAS 能力在 CommitAbility（提交技能）阶段执行真实成本/冷却效果，技能数据配置**不会自动变成 GAS GameplayEffect**；必须为每一个正式技能实装并核验该步骤，本轮只是具备读取数值、可信角色校验与提交 CombatSpec 的代码入口。
6. UDivineBeastsConfiguredGameplayAbility（项目技能基类）不定义鼠、龙等业务动作子类；策划批准后优先用同一 C++ 技能模板/蓝图派生实例绑定不同资产。项目正式禁令：不恢复 Element（旧五行）、克制、破元、共鸣机制。
7. 本轮动态 AbilitySet 授权只支持正常 AbilityGrant 与合法可撤销启动 EffectGrant；新增动态 AttributeGrant 会明确拒绝。基础 Momentum（气势）及通用战斗属性由各原有初始化组件维护，不在技能组件中建立不可回滚的第二套属性集。

## 授权与生命周期

- UDivineBeastsCharacterComponent（角色身份组件）向服务器发布真实就绪事实，Loadout（技能装配组件）订阅此事件；仅服务器可发起 AcquireDefinition 和 GAS GiveAbility。
- 完成异步加载后再校验 Set、预热 AbilityClass、输入标签与当前 ASC 其他授权是否冲突。全部授予成功才发布 Ready=true。发生任一失败，撤回本次创建的 AbilitySpec/GameplayEffect；其它来源（如装备）的技能不受影响。
- 新增同一个 ASC 内技能类重复检查，并要求 UDivineBeastsConfiguredGameplayAbility（数据驱动技能基类）的 AbilityDefinitionId（逻辑主资产 ID）与 AbilitySet 中授权的 AbilityId 完全一致；技能玩法定义必须属于当前已确认的英雄且对应级数表已预热且合法，否则服务器拒绝授权。
- HeroDefinitionId（英雄编号）、AvatarGeneration（角色代次）、RequestSerial（本地请求代次）及 LeaseId（数据租约编号）共同隔离迟到回调；EndPlay、离开、换英雄撤销并释放本组件租约。
- 仅向 Owner（拥有者）复制最小的 FDivineBeastsAbilityLoadoutState（技能列表及等级、输入与槽位），不给客户端复制真实技能类引用或可修改的伤害数据。
- 尚缺在 OpenWorld/Village 公共出生路径的真实可玩 Pawn 装配、断线重连全链验证、以及正式技能资产授权；这些是后续必过门禁，不可记作完成。

## 正式资产尚缺（不可写伪 .uasset）

- 12 个已批准 DefaultAbilitySetId 数据填充、真实 AbilitySet、每级技能定义和 DataTable（数据表）、技能行为蓝图/Effect。
- 计划第一阶段只要求经过策划审核的一个子鼠技能，再以辰龙覆盖多阶段/远程范围；不预先宣称已经存在60个正式技能。
- Server Cook（专用服务器资源烘焙）仅允许玩法 Definition、必要 GameplayEffect/Animation；图标、Widget、VFX/SFX 不在 DBAGameplay 的资源中。
- 归属设置校验通过仅表示数据结构可加载；真正的 CooldownSeconds/MomentumCost（冷却与气势成本）还必须由已批准的 GAS Ability / GameplayEffect 消费，禁止仅填数值表而宣称施法会自动扣气势、进入冷却。

## 2026-10-09 增量实现：授权失效与数值对账

- `UDivineBeastsCharacterComponent::AuthorityBindTrustedContext`（可信身份重新绑定）首先发布 `ReadinessChanged(false)`（旧角色失效事件），然后更换英雄身份与出生代次。项目技能组件据此撤销自己的旧 GAS 授权、GameplayEffect（玩法效果）和 Data Lease（数据租约），防止换英雄残留旧技能。已写入源码，仍待实际 UE 编译及重生测试。
- `UDivineBeastsAbilityLoadoutComponent::ApplyAbilitySet`（授权前校验）按获授的真实技能等级，核对技能表的 `MomentumCost`（气势成本）与实际 GameplayEffect 修改幅度一致：只接受 `Instant`（瞬时）负向 `Additive`（加法）气势修改；对其他未声明资源修改一律拒绝。`CooldownSeconds`（冷却秒数）与合法 `HasDuration`（限时）GameplayEffect 的持续秒数相同，并要求冷却 Tag（标签）存在。仅支持可由 `GetStaticMagnitudeIfPossible`（静态幅度计算）求值的配置，容许 0.01 误差；动态 `SetByCaller`（运行时指定数值）需要单独审核，不默许数值漂移。
- `UDivineBeastsConfiguredGameplayAbility`（技能基类）每次激活清空提交状态，仅 GAS `CommitAbility`（正式提交成本及冷却）成功后允许服务端调用 `AuthorityApplyConfiguredDamage`（可信命中伤害）；技能结束或取消后撤销该资格。不等于已经实现正式技能的攻击动作或特效。
- 十二生肖内容包已另行产生 60 枚 PNG 图源（每英雄五枚）；不是正式 AbilityId 或 UE Texture2D。源图校验脚本 `Tests/Assets/ValidateZodiacSkillIconSources.py` 与各英雄包清单只检查文件及 SHA-256，不代表图标导入/绑定或运行验收。
