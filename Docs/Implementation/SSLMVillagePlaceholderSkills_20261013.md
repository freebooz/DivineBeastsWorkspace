# SSLM Village 十二生肖默认技能占位配置与验证记录

- 日期：2026-10-13；工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（SSLM正式工程）
- 状态：**12套占位默认技能 UE5.8 原生资产已制作并独立冷启动校验通过**。不代表完成专用服务器联机、技能真实命中与客户端/服务器 Cook 验收。
- 范围：仅既有第三层 `DBAGameplay`、`DBAClient` 内容包；平台层 `GamePlatformAbilitySystem/GamePlatformCombat/GamePlatformData` 和第二层 MOBA 无新造同名实现。
- 对其他工作区未提交改动、其它技能开发夹具和旧稳定身份均保持隔离；占位命名中明确 `_placeholder@1`，不宣称设计获批。

## 一、12生肖共享五槽临时模板

| 槽位 | 临时机制 | 一级基础伤害 | 气势消耗 | 冷却 | 射程 | 操作 |
|---|---|---:|---:|---:|---:|---|
| 普通攻击 | 前向单目标权威攻击 | 40 | 0 | 0秒 | 250cm | 普攻键 |
| 被动 | 不触发主动效果的暂占槽 | 0 | 0 | 0秒 | 0cm | 无 |
| 技能一 | 火焰弹，单枚直线投射 | 40 | 10 | 2秒 | 2000cm | 键1 |
| 技能二 | 寒冰箭，单枚直线投射 | 30 | 8 | 1.5秒 | 2000cm | 键2 |
| 终极技能 | 奥术飞弹，3枚螺旋弹体，每枚基础12伤害 | 12×3 | 12 | 3秒 | 2000cm | 键4 |

说明：以上数字只用于当前占位调试，不是十二生肖最终数值批准；没有燃烧、冻结、减速、位移或额外被动效果。正式战斗伤害继续由平台 Combat 统一结算，技能先经过 GAS 服务端 Commit（提交）后才生成投射物；客户端不授予技能。

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
