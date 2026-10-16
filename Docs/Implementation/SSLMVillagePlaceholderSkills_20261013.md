# SSLM Village 十二生肖默认技能占位配置与验证记录

- 日期：2026-10-13；工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（SSLM正式工程）
- 状态：**12套占位默认技能 UE5.8 原生资产已制作并独立冷启动校验通过**。不代表完成专用服务器联机、技能真实命中与客户端/服务器 Cook 验收。
- 范围：仅既有第三层 `DBAGameplay`、`DBAClient` 内容包；平台层 `GamePlatformAbilitySystem/GamePlatformCombat/GamePlatformData` 和第二层 MOBA 无新造同名实现。
- 对其他工作区未提交改动、其它技能开发夹具和旧稳定身份均保持隔离；占位命名中明确 `_placeholder@1`，不宣称设计获批。

## 一、12生肖共享五槽临时模板

| 槽位 | 临时机制 | 一级基础伤害 | 气势消耗 | 冷却 | 射程 | 操作 |
|---|---|---:|---:|---:|---:|---|
| Primary兼容槽 | 花瓣绽放：自己或选中有效友方恢复目标最大生命20% | 0（治疗） | 0 | 10秒 | 2000cm | 键1 |
| 被动 | 不触发主动效果的暂占槽 | 0 | 0 | 0秒 | 0cm | 无 |
| 技能一 | 火焰弹，选中目标追踪／未选直线 | 40 | 0 | 2秒 | 2000cm | 键2 |
| 技能二 | 寒冰箭，选中目标追踪／未选直线 | 30 | 0 | 1.5秒 | 2000cm | 键3 |
| 终极技能 | 奥术飞弹，3枚螺旋弹体，每枚基础12伤害 | 12×3 | 0 | 3秒 | 2000cm | 键4 |

说明：2026-10-15按用户明确指令将全部法术气势费用归零，并增加选中目标冻结追踪；原伤害／冷却／射程不变，飞行中及连发不随新选择切敌，冻结目标失效终止。以上数字只用于当前占位调试，不是十二生肖最终数值批准；没有燃烧、冻结、减速、位移或额外被动效果。正式战斗伤害继续由平台 Combat 统一结算，技能先经过 GAS 服务端 Commit（提交）后才生成投射物；客户端不授予技能。

- 12个英雄分别配置 `dba.abilityset.<hero>_placeholder@1`；各自拥有5个独立 `dba.ability.<hero>_placeholder_<mode>@1`，互不复用另一生肖的技能身份。
- 能力原生类：`DivineBeastsPlaceholderGameplayAbility`（占位技能执行器），在第三层 `DivineBeastsAbilitiesRuntime` 模块。
- 数据目录：`/DBAGameplay/Abilities/Placeholders`（服务端安全定义/GAS资产）；图标/Profile（客户端表现配置）位于 `/DBAHeroPack_<Hero>/UI/Abilities`，复用三枚 `/DBAUIPack_Core/UI/Abilities/MagicTests` 法术图标。
- 数据流：可信英雄定义 `DefaultAbilitySetId` → 平台 DataService 异步租约 → 服务器 GAS 授予/预检 → OwnerOnly 授权快照及平台 GAS Spec 复制 → 客户端只读技能栏/图标 → 输入激活与服务器权威战斗。
- 生命周期：换角色先撤销旧技能资格再广播新英雄身份；旧异步租约回调、旧AvatarGeneration不能重新授权。失控、死亡、跨世界清理由既有平台生命周期和组件撤销处理，**仍待真实联机场景验证**。

## 二、真实 UE5.8 资源交付与证据

第一阶段 `AuthorZodiacPlaceholderFoundation.py` 由UE编辑器生成并保存 **145份资产**：60技能定义、36成本 GE（效果）、36冷却 GE、12 UIProfile、1数值表。原生Python脚本执行成功，但首次查询不存在的目标文件触发 UE 错误日志导致该次命令行返回非零；此记录不是未保存，实际文件和后续冷读均已核对。

第二阶段 `AuthorZodiacPlaceholderAbilities.py` 由引擎生成 **60个独立GA蓝图、12套授权集**，重新保存已有基础资产及12份正式Hero默认集合。第二阶段命令行 **退出码0，0错误，4条已存在的运行环境警告**。磁盘现有技能目录205份资源，加另12份 UIProfile；原12份英雄 Definition 已修改为各自默认占位技能 ID。

独立冷读验证：`Tests/Assets/ValidateZodiacPlaceholderUE.py` 通过 UE5.8 PythonCommandlet 加载真实资产，输出 `Game/Saved/Validation/VillageSkillPlaceholder20261013/ColdReadback.json`，状态 PASS、0错误，计数 12 Hero/60 Ability Definition/60 GA Blueprint/12 Ability Set/12 UIProfile。

仅供本地工作区证据的 `Saved` 报告，不应把文件名或成功退出码替代专服真实授权、GAS释放与双端网络复制证据。

## 三、构建与测试门禁

- UE5.8 Editor `DivineBeastsArenaEditor Win64 Development` 已真实增量构建成功（新占位原生模块编译链接成功）。第一次跨 DLL 链接遇 `NetCore` 文件锁，命令行UE编辑器退出后重复构建成功。
- `Tests/Architecture/ValidateZodiacAbilityIntegration.py`（技能三层静态门禁）41项通过、0失败。
- `Tests/Assets/TestZodiacAbilityDelivery.py`（资产账簿失败关闭单元测试）8项通过。
- `Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory`（存在性）通过；`--release`（正式设计和上线门禁）**仍按预期列出70个阻塞，退出码2**。临时 `_placeholder` 不能当作已获批的正式技能设计。
- UE启动期间另有4条警告：缺开发工具Allowlist、两个重复 Foundation 探针主资产ID、一个 Python 反射命名冲突；该批次没有 UE Python 错误。
- 客户端/服务器独立构建与干净 Cook/Stage、真实 Village 单/双客户端专服入服授权、技能栏可视化、伤害及断线重连，必须以另行获取的运行证据填写；未实际执行的项目不得改为通过。

## 四、正式技能替换策略

正式生肖技能获批时：先在第三层独立新建最终能力定义/数值及其实现与客户端Profile，确保GamePlatformData租约和 GAS 校验通过；再按角色逐个修改 `DefaultAbilitySetId` 指向已批准的新 AbilitySet。旧 `_placeholder` 身份保留可回溯性，清除未使用的临时资源必须另经引用/Cook审查，不能直接重用已发布技能编号或覆盖现有开发魔法资产。

**后续重点回归**：首次进入 Village、切换角色、死亡重生、断线重连、异步主资产获取失败、跨世界切换；以及两端Cook必需Definition和客户端专属图标/VFX的正反向排除检查。需分别验证服务器GAS授予数、客户端OwnerOnly槽位、实际GAS成本/冷却与 Combat伤害、复制弹体表现、异常回滚和代次幂等。

## 五、运行证据索引

- 第一批报告：`Game/Saved/Validation/VillageSkillPlaceholder20261013/FoundationReport.json`
- 最终制作报告：`Game/Saved/Validation/VillageSkillPlaceholder20261013/AuthorReport.json`
- 独立冷读结果：`Game/Saved/Validation/VillageSkillPlaceholder20261013/ColdReadback.json`
- 原生模块源码：`Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/{Public,Private}/Abilities/DivineBeastsPlaceholderGameplayAbility.*`
- 正式12生肖与占位关系台账：`Docs/Implementation/ZodiacAbilityAssetInventory.json`

## 2026-10-15 零费用合同续修

实际GAS目标连发回归暴露原36法术行仍保留费用，当前平台Magic零费校验因此拒绝施法。通过正式UE5.8 Monolith在原资产身份下修正36行、36中性兼容成本GE、36GA空成本引用及12UIProfile说明；成本原始快照和保存记录在`Game/Saved/Validation/SpellMomentumMigration`，真实uasset备份在`Saved/Validation/WorldEntryFix20261015/SpellAssetsBefore`。两个作者入口共同复用零费SLOTS，保留原GAS冷却，不靠补资源实现免费。独立只读检查现逐行验证36条Magic、36份中性GE及保留冷却，采用唯一时间戳报告；最终冷读与原生运行记录由本轮执行报告补充，不沿用2026-10-13旧费用合同结论。

本轮独立原生重载85包后只读验证PASS：12英雄、60定义／GA、12技能集／UIProfile、36条Magic费用为0、36中性GE及36原冷却合同通过。随后最终33项UE回归全部成功，包含两类真实GAS连发的冻结选择／销毁并GC／目标换代，并断言气势始终等于真实初值；原始聚合`Saved/Validation/WorldEntryFix20261015/FinalNativeVerified.json`。源码Client／Server与Editor新构建均退出0，最终包与联机仍须看本轮后续Cook／运行记录。

## 2026-10-16 数字1治疗接线修复

最新双打包客户端与Village专服使用`VillageDevelopmentClient/Server`及`-DBADevelopmentSkills`。旧配置把十二英雄覆盖到`dba.abilityset.<hero>_dev@1`，`DivineBeastsAbilityLoadoutComponent`因此跳过英雄已迁移的`_placeholder@1`默认集合；数字1仍授予历史候选普攻，不能进入`Healing`或产生花瓣治疗确认。这是运行配置与真实授权链的冲突，不能通过重复导入Niagara修复。

本次两份开发配置移除旧候选集合映射，保留双重开发许可和原有诊断/Cook范围，运行时沿用英雄`DefaultAbilitySetId`。客户端`DivineBeastsAbilityUIProfile`扫描同步改为十二英雄包的`UI/Abilities`，避免用旧候选Profile解释当前占位技能ID；开发候选资产和原生显式覆盖机制保留，独立验证需要另行明确配置，普通联调不自动替换技能。

权威治疗继续复用`DivineBeastsPlaceholderGameplayAbility`及平台`ApplyHealing`，最大生命20%、10秒GAS冷却、零气势费用、友方资格和失效拒绝规则均不变。受疗确认映射到`Presentation.DivineBeasts.Development.Magic.Healing.Impact`，Definition为`/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_HealingPetalBloom`，引用已迁移的`/DBAPresentationPack_Core/VFX/Library/07_Status/NS_Nature_PetalBloom_Aura`，不在客户端自行加血。

项目客户端适配补充Standalone/ListenServer的本地权威Combat事件桥接，交给既有World反馈总线按EventId去重，保证源与受疗者的两次同一确认只播放一次。首次绑定只读取存量Actor，此后由新增、EndPlay与Combat事件维护，世界解绑撤销全部订阅；网络客户端仍只消费专服确认。MainArena在既有双重开发许可下沿用同一映射，不新增通用地图白名单或专服表现依赖。

已执行`Tests/Assets/TestZodiacDevelopmentAbilityAuthoring.py`：修复前新增合同出现2处预期失败，修复后4项通过。`DivineBeasts.Presentation.Healing.TargetAndLifecycle`新增真实本地主机`ApplyHealing`、源/目标重复确认及解绑回归，本次子任务未构建或执行UE；由主任务统一完成固定参数增量构建、真实资产重载、打包与专服/客户端验收。原`DivineBeasts.Abilities.Placeholder.ServerGAS.PrimaryHealing`仍是权威治疗的最小回归入口。中文说明核对范围限于本次配置、表现适配、依赖声明及相关测试，不宣称历史源码全量合规。
