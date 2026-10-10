# DBAGameplay：十二生肖技能数据驱动与权威授权

> 适用工程：DivineBeastsWorkspace（神兽联盟工作空间）。
> 当前实施状态：已写入 C++ 源码及部分装配，**未通过锁定 UE5.8 引擎编译与真实资产/联机验收**。
> 依据：根 AGENTS.md（工程规则）、Game/Plugins/插件开发规范.md（插件规范）。

## 模块归属和三层边界

- GamePlatformAbilitySystem（平台 GAS 技能系统）保留原生 ASC、AbilitySet 和 FGamePlatformAbilityGrant（单项技能授权）稳定契约；GamePlatformCombat（平台战斗）保留伤害计算与命中验证；GamePlatformData（平台数据）负责逻辑主资产 ID 和生命周期租约。
- MobaCommon（MOBA 可选中间层）不新增生肖技能类；竞技特有效果只在 MobaPresentation（MOBA 表现）把竞技事实映射成中立语义。
- DBAGameplay（神兽联盟项目玩法插件）新增 **DivineBeastsAbilitiesRuntime（技能运行模块）**，其中 UDivineBeastsAbilityDefinition（技能逻辑资产）与 FDivineBeastsAbilityBalanceRow（每级数值行）负责玩法数据；UDivineBeastsAbilityLoadoutComponent（角色技能装配组件）根据服务器已认可的 HeroDefinition（英雄定义）中的 DefaultAbilitySetId（默认技能集编号）加载现有 AbilitySet 并授权技能。
- DivineBeastsCharactersRuntime（角色身份模块）只新增 FName DefaultAbilitySetId（稳定逻辑编号），不引用具体技能类、逻辑定义、UI 或图标；新运行模块依赖角色模块，不反向依赖，插件关系保持单向。
- DBAServer（服务器控制面）本轮没有新增全球单例技能管理器；项目实例出生/准入需要在 OpenWorld 与 Village 的真实 Pawn 组合点采用 ADivineBeastsGameplayCharacter（可玩角色：继承唯一权威Pawn并增加技能装配）或显式安装同等权威组件。当前只将竞技场服务器的出生原型改为此类，其他角色还不能宣告打通。

## 真实公开类型（新增源文件，待 UE 编译）

- Public/Types/DivineBeastsAbilityBalanceRow.h（技能数值表字段和单位）。
- Public/Definitions/DivineBeastsAbilityDefinition.h（项目技能定义；继承 GamePlatformDefinitionBase，不复制 LogicalId）。
- Public/Abilities/DivineBeastsConfiguredGameplayAbility.h（数据驱动技能基类：只读预加载数值，服务器调用 GamePlatformCombat 伤害接口）。
- Public/Components/DivineBeastsAbilityLoadoutComponent.h（OwnerOnly 技能授权事实复制；带当前 Hero ID、AvatarGeneration 和 Revision）。
- Public/Characters/DivineBeastsGameplayCharacter.h（继承角色模块的ADivineBeastsCharacter，复用唯一ASC/角色身份/Combat/Gameplay资格及死亡、失控、退出生命周期，仅新增技能Loadout）。
- Private/Tests/DivineBeastsAbilityDefinitionTests.cpp（技能数值字段校验）、Private/Tests/DivineBeastsAbilityAssemblyTests.cpp（反射和默认拒绝状态）。

## 数据配置合同与单一真源

HeroDefinition.DefinitionId（英雄定义编号） → HeroDefinition.DefaultAbilitySetId（默认技能集合逻辑编号） → GamePlatformDefinition:AbilitySet（平台主资产） → AbilityGrant.AbilityId（规范技能身份）、AbilityGrant.AbilityClass（GAS 技能类）、AbilityGrant.InputTag（输入标签） → AbilityClass.AbilityDefinitionId（项目玩法定义主资产） → AbilityDefinition.BalanceTable 与 LevelRowNames（权威每级数值表）。

1. DefaultAbilitySetId 为空时保留旧英雄资产的读取兼容性，但不给出任何技能，明确报告未配置。不能为十二生肖制造未经批准的正式 AbilityId、技能名或伤害数字。
2. FGamePlatformId（平台逻辑身份）规范形如 namespace.name@version（命名空间.名称@版本），并非 .uasset 文件路径。新 UDivineBeastsAbilityDefinition 继承 LogicalId，不再定义第二个 AbilityId；Balance 行与技能授权使用该规范字符串互相校验。
3. **当前技能数值表字段已精简**：Level（等级）、BaseDamage（基础伤害）、DamageType（技能伤害类型/表现语义）、CooldownSeconds（冷却秒数）、MomentumCost（气势消耗）、CastRangeCm（厘米施法范围）、AreaRadiusCm（厘米范围半径）。不再有攻击/技能强度系数或暴击开关；各类Buff/Debuff对伤害的增减均归平台Combat属性和结算规则。输入、单位、合法区间由Row.Validate校验，旧DataTable字段需资产兼容审核。
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

### 2026-10-10 启动 GameplayEffect 授权的事务回滚安全约束

- `UDivineBeastsAbilityLoadoutComponent::IsStartupEffectReversible`（服务器启动效果可撤销校验）在 `GiveAbility`（授予技能）前拒绝 `Instant`（瞬时效果）、有周期执行的 `GameplayEffect`（玩法效果）、自定义 `Executions`（执行计算）、非 `None`（不堆叠）的堆叠策略及尚未经过专门安全审核的 `GameplayEffectComponent`（附加效果组件）。防止在授权半途失败时，由瞬时生命变化、定时计算、额外子效果或其他来源堆叠留下不可逆副作用。
- 当前第一批只允许无周期、无执行计算、无附加组件且非堆叠的有限时长或无限时长启动效果。Buff/Debuff（增益/减益）的更多原生效果组件支持，必须先建立白名单和独立的回滚测试，再扩大范围，不得在游戏运行期自行放开。
- `ApplyAbilitySet`（应用技能集合）现在**先预检全部素材与效果，再先应用可回滚的启动效果，最后授予技能Spec**，与平台 `EffectGrantPolicy::WhileGranted`（效果跟随授权）和既有“效果→技能”的公共契约保持一致；错误时调用 `RevokeOwnedGrants`（撤销本组件授予），只按本组件保存的真实句柄释放，不清空外部GAS授权。
- UE5.8 中 `UGameplayEffect::GEComponents`（附加效果组件数组）为引擎受保护字段，本项目通过官方公开的 `FindComponent(UGameplayEffectComponent::StaticClass())` 判断任意附加组件存在，不越过 Unreal Engine（虚幻引擎）C++ 类封装。
- 新增独立 `DivineBeasts.Abilities.GrantTransaction.ReversibleStartupEffect`（技能授权事务可撤销性）UE自动化用例，覆盖合法无限效果与瞬时、周期、执行计算、堆叠负例。该用例只读取世界无关的瞬态GameplayEffect并通过UE原生反射设置编辑器专属堆叠字段，不跨模块链接未导出的编辑器设置API。**源码已经写入，须等新DLL生成并重新加载编辑器后才可能在测试列表中出现，不将旧DLL的2项技能测试误认作新增用例通过。**

## 正式资产尚缺（不可写伪 .uasset）

- 12 个已批准 DefaultAbilitySetId 数据填充、真实 AbilitySet、每级技能定义和 DataTable（数据表）、技能行为蓝图/Effect。
- 计划第一阶段只要求经过策划审核的一个子鼠技能，再以辰龙覆盖多阶段/远程范围；不预先宣称已经存在60个正式技能。
- Server Cook（专用服务器资源烘焙）仅允许玩法 Definition、必要 GameplayEffect/Animation；图标、Widget、VFX/SFX 不在 DBAGameplay 的资源中。
- 归属设置校验通过仅表示数据结构可加载；真正的 CooldownSeconds/MomentumCost（冷却与气势成本）还必须由已批准的 GAS Ability / GameplayEffect 消费，禁止仅填数值表而宣称施法会自动扣气势、进入冷却。

## 2026-10-09 增量实现：授权失效与数值对账

- `UDivineBeastsCharacterComponent::AuthorityBindTrustedContext`（可信身份重新绑定）先撤销旧Ready和自有定义租约，发布当前受信新身份，再仅广播一次`ReadinessChanged(false)`（资格/旧技能撤销事件）。FGuid操作身份在租约取消、同步通知及初始化之后重查；监听者绑定更高代次或退出后，原栈返回撤销，不能覆盖后继身份或继续加载。技能组件只撤销自己的GAS授权、玩法效果与租约；真实重入/通知内关闭回归纳入中央UE验证，不将源码断言当运行通过。
- `UDivineBeastsAbilityLoadoutComponent::ApplyAbilitySet`（授权前校验）按获授的真实技能等级，核对技能表的 `MomentumCost`（气势成本）与实际 GameplayEffect 修改幅度一致：只接受 `Instant`（瞬时）负向 `Additive`（加法）气势修改；对其他未声明资源修改一律拒绝。`CooldownSeconds`（冷却秒数）与合法 `HasDuration`（限时）GameplayEffect 的持续秒数相同，并要求冷却 Tag（标签）存在。仅支持可由 `GetStaticMagnitudeIfPossible`（静态幅度计算）求值的配置，容许 0.01 误差；动态 `SetByCaller`（运行时指定数值）需要单独审核，不默许数值漂移。
- `UDivineBeastsConfiguredGameplayAbility`（技能基类）每次激活清空提交状态，仅 GAS `CommitAbility`（正式提交成本及冷却）成功后允许服务端调用 `AuthorityApplyConfiguredDamage`（可信命中伤害）；技能结束或取消后撤销该资格。不等于已经实现正式技能的攻击动作或特效。
- 十二生肖内容包已另行产生 60 枚 PNG 图源（每英雄五枚）；不是正式 AbilityId 或 UE Texture2D。源图校验脚本 `Tests/Assets/ValidateZodiacSkillIconSources.py` 与各英雄包清单只检查文件及 SHA-256，不代表图标导入/绑定或运行验收。

## 2026-10-09 实际技能资产与发布门禁复核

- Monolith 0.23.0（虚幻编辑器界面工具）已回读十二个生肖英雄真实主资产，`DefaultAbilitySetId`（默认技能集编号）全部为 `None`。即使代码具备授予流程，**在未绑定真实 AbilitySet（技能授权集合）前，不得声明任何生肖已可施法或结算伤害**。
- 十二生肖内容插件的60枚图标已实际导入为 `Texture2D`（真实纹理资产）；`DBAUIPack_Core`（项目公共界面包）中的技能栏、技能单格两份真实 Widget Blueprint（控件蓝图）分别经 Monolith 新编译，均为0错误、0警告。此为新一轮引擎资产证据，更新上方早期“只有PNG”的历史状态；正式 AbilityId（技能编号）、UI Profile（技能界面配置）和服务器授权尚缺。
- 新增 `Tests/Assets/ValidateZodiacAbilityDelivery.py`（十二生肖技能正式交付只读门禁），`--inventory`（文件实物完整性）退出0，`--release`（生产技能与运行验证）因真实70项缺口按预期退出2。12个英雄各五项正式技能主数据、客户端图标绑定、GAS授权、UE编辑器/客户端/专用服务器编译、Automation（自动化）、双客户端联机、Cook（资源烘焙）均必须补证后才能变更发布状态。
- 本轮 UBT（虚幻构建工具）尝试 `DivineBeastsAbilitiesRuntime`（项目技能运行模块）Editor定向编译，UHT（反射生成）处理通过，随后编译143个Action（动作），在1200秒执行上限被中止，没有成功链接证据；不可宣称模块已通过真实C++编译。详见 `Docs/Implementation/十二生肖技能数据驱动实施记录_20261009.md`（项目实施台账）。


## 2026-10-09 主分支整合兼容边界

`ADivineBeastsGameplayCharacter`反射类和模块身份保持；公开BeginPlay/PossessedBy/GetAbilitySystemComponent保留转发符号，OnRep_PlayerState沿用服务器无Controller失败关闭。默认子对象AbilitySystem/Combat/AbilityLoadout保留同名，原独立HeroIdentity由继承的CharacterIdentity替代，并新增继承GameplayEligibility；不再并列构造两套ASC或死亡执行器。继承关系与唯一组件、真实Possess/UnPossess由现有Assembly.Contract回归校验，Blueprint序列化模板、Cook及网络兼容仍需真实资产补证；本轮没有迁移或伪造资产。回退应同步撤回该角色继承、出生选择和状态通知操作身份，不只恢复单个函数。

主线新增模块留在既有DBAGameplay，当前代码/机制插件仍46个，内容16另计；83插件模块加主模块84。上方早期未编译、仅PNG等记录保留历史边界，实际当前构建与逐项运行结果以`Docs/Implementation/GamePlatformAuditRemediation/UEBuildResults.json`和`UEAutomationResults.json`为准；不能据此宣称60个正式技能已批准上线。
## 2026-10-09 统一开发样板资产与正式隔离门禁

- `/Game/Development/DivineBeasts/Abilities`（仅用于编辑器的技能开发资源目录）现由真实UE编辑器工具生成60份 `DivineBeastsAbilityDefinition`（生肖技能逻辑数据资产）与 `DT_DBA_Zodiac_DevBalance`（60行一级测试技能平衡数据表）。每个英雄五个槽位，各自拥有独立 `FGamePlatformId`（稳定技能身份）；60个一级行名按 `Hero_Slot_L1`（英雄_槽位_一级）的统一结构关联。另有12份 `DivineBeastsAbilityUIProfile`（开发技能图标/名称配置）、子鼠开发 `GameplayAbility`（技能蓝图）和 `AbilitySet`（能力授予集合）。所有数据均为 **开发样板**，不是正式策划批准的技能生产资产。
- 开发样板保留两份数据表文件：`DT_DBA_Rat_DevBalance`（历史子鼠演示表，停止作为当前技能定义真源）与 `DT_DBA_Zodiac_DevBalance`（当前唯一开发数值表）；子鼠普攻定义已切换至统一表的 `Rat_BasicAttack_L1` 行。完整实际文件及 SHA-256 见 `Docs/Implementation/ZodiacDevelopmentUEAssetEvidence_20261009.json`（开发资产证据清单）。
- `Tools/Unreal/Abilities/ValidateZodiacDevelopmentAssets.py`（UE5.8编辑器原生验证脚本）已在 Monolith `editor.run_python`（编辑器脚本接口）运行，核对**60份实际逻辑定义、60个数值行、12份开发UI配置和60条UE纹理软引用**，结果为0错误。`Tests/Assets/ValidateZodiacDevelopmentSkillAssets.py`（不依赖引擎的文件及摘要门禁）使用 `--require-all-profiles --require-all-definitions --verify-hashes`（全量检查）通过，但不能替代真实引擎。
- `Game/Config/DefaultGame.ini`（项目默认配置）将整个开发资源目录加入 `DirectoriesToNeverCook`（发行资源烘焙排除清单），并将 `bAllowDevelopmentAbilitySets=false`（开发技能集授予开关）设置为默认值。`UDivineBeastsAbilityLoadoutComponent`（项目权威授权组件）新增编译期及配置双门禁：仅UE编辑器显式启用开发权限才可能授予 `bDevelopmentOnly`（开发集合），正式客户端、专用服务器及发布目标均拒绝。生产英雄 `DefaultAbilitySetId`（默认技能集编号）仍保持空值，不使用此开发样板代替。
- 已知边界：当前只有一份子鼠可激活的开发 `GameplayAbility`（GAS技能蓝图），它只提交成本/冷却与正常结束，并不执行真正的目标选择、命中或伤害；另外59份技能逻辑定义不能当成已具备可执行技能类。正式 `DataAsset`（玩法资产）、`GameplayEffect`（技能效果）、VFX/SFX、全英雄授权、UI运行显示、三端编译、联机与Cook/Stage须分别验收，不能由全量开发数据反推生产功能完成。
## 2026-10-09 主线整合同步通知与运行资格整改

角色定义配置调用真实Capsule重叠、碰撞Profile、Combat重置及GAS属性后，均可能同步调用项目监听者。配置执行与预热交接各自捕获原初始化身份、Owner/World、定义和出生/Avatar代次；每次外部调用后重验，最终提交配置/Ready前再验。后继接管或生命周期结束时，旧栈只停止剩余写入，不把已经发生的引擎/GAS修改假装回滚。

`IsCharacterReady`（当前可运行角色资格）除既有资源租约及端侧Ready条件，还要求组件已注册并进入BeginPlay且未结束。真实UnregisterComponent不会自动EndPlay，GetWorld可从Owner回退；因此仅凭可读资源与身份字段，不能证明组件仍能参与玩法。平台只读StateView和项目ActivationGate沿用同一查询，注销期间失败关闭，合法重新注册的生命周期仍需重新核验。

竞技出生使用已受理的可信上下文操作ID作为观察身份；它不是准入凭证。Restart、初始化、预热交接、Avatar绑定及资格激活通知返回后，均核原Adapter/比赛阶段、Controller/Pawn、完整上下文及操作身份。初始化失败无法取得原内部ID时保留未证明归属的角色；后续明确自有的正常失败才可清理。最后实际读取Ready并再次核同一作用域；失效只撤本次死亡监听，不清后继监听或资格。具体真实配置/出生/激活回归和构建结果见中央UE证据，源码闭环不等于联机、Blueprint兼容或Cook验收。
