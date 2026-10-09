# 3A战斗状态效果UI设计规范 V1.0（Buff增益 / Debuff减益）

> 日期：2026-10-09。适用GamePlatformUI（游戏平台UI）、GamePlatformArenaClient（MOBA竞技UI）、DivineBeastsUIClient（项目公共UI）、DivineBeastsArenaClient（项目竞技UI）。
> **本文件是研究与设计提案，非代码已修改、蓝图已交付或运行已通过的证明。** 外部官方参考链接见《3A游戏UI十大业务域组件总清单_V1.0.md》。
> 来源参考：《魔兽世界》HUD编辑/冷却管理、《最终幻想XIV》增减益与团队状态、《命运2》关键状态通道、《无畏契约》增减益分区。

## 2026-10-09 源码状态校准（最新实施证据优先）

之前的“尚缺实例身份、驱散、分组、重要性”等条款为初始设计差异；经重新读取正式`GamePlatformStatusEffectTrayWidget.h/.cpp`，这些中立展示能力**已经在现有C++实现中**：

- `EffectInstanceId`（状态效果实例ID）、`SourceDisplayId`（已获授权的施放者展示身份）、`Polarity`（增益/减益/中性/条件显示性质）、`PrimaryMechanic`（控制/免疫等机制）、`VisibleMechanicTags`（已准许展示标签）；
- `Importance`（视觉重要性）、`DispelCategory`（驱散类别）、`bLocallyDispellable`（本地玩家可尝试驱散的展示事实）、`bCriticalMechanic`（关键机制）、`ExpirationTimeAnchorSeconds`（显示时间锚点）和`SourceScopeId`（来源代次）；
- `BuildDisplayGroups`（增益、减益、关键警报、其他四组）、`MaxBeneficial/MaxHarmful/MaxCritical/MaxOther`（各组图标容量）和`OverflowCount`（溢出数量）、同效果多实例区分与稳定排序。

本次另新增`GamePlatformCastProgressWidget`（施法/引导条）、`GamePlatformTargetFrameWidget`（目标/焦点状态框）和`GamePlatformCombatAlertWidget`（关键战斗预警）三个独立跨游戏显示组件，已通过UE5.8单源文件编译。仍待落实的是真实GAS/竞技客户端事件适配、观察者可见性过滤、目标实例作用域、共享显示倒计时调度、Monolith蓝图制作、运行联机与Cook验证。**以下初始字段提议表仍供兼容性审阅，不可视为当前未编码清单，也不声明整套战斗UI已完成。**

## 一、状态效果分类：不能简单按是否有益一分为二

- **Buff（增益）**：加速、增伤、强化、护盾、恢复、免疫、条件触发等，状态是否生效由战斗服务/GAS（能力系统）确认。
- **Debuff（减益）**：减速、减防、持续伤害、易伤、沉默、禁锢、眩晕等，其可见性同样由已授权客户端事实决定。
- **CC / Crowd Control（控制）**：Stun（眩晕）、Silence（沉默）、Root（定身）、Knockback（击退）、Knockup（击飞）、Fear（恐惧）、Taunt（嘲讽）、Blind（致盲）、Disarm（缴械）。是机制分类，不是另一套权威状态系统。
- **DoT（持续伤害）与HoT（持续治疗）**：效果的生效方式，不等于正负分类，图标计时与战斗飘字要明确区分。
- **Shield（护盾）**：数值变化由ResourceBar（资源条）展示，持续效果可另有图标，避免仅用图标解释数值。
- **Dispel（驱散）**：是否可驱散、驱散类别与本地玩家是否拥有资格属于三个不同事实；显示不等于允许执行驱散。
- **Immunity（免疫）**：免伤、免控、不可打断等；图标必须配形状/文字，不能单靠颜色。
- **Proc（触发状态）**：技能可用窗口、免费施放、连招强化等，与技能槽联动，不在UI里重算触发条件。
- **Cooldown（技能冷却）**：技能层的冷却和状态效果剩余时间来源不同，可以复用通用倒计时视觉基类，不能合并权威状态模型。
- **Positive/Negative/Neutral/Conditional（有利/不利/中性/条件性）**与MechanicTags（机制标签）是两个正交分类维度；永久光环、短期Buff、Boss危险机制可用同一基础数据结构投影。

## 二、建议组件族（唯一主归属：Combat战斗域）

| ID（组件标识） | 中文职责 | 当前实现/建议 |
| --- | --- | --- |
| `UI.Combat.StatusEffectTray` | 状态效果统一容器 | 已有UGamePlatformStatusEffectTrayWidget（平台状态托盘C++）；项目蓝图未交付 |
| `UI.Combat.BuffTray` | 增益分区 | 复用同托盘，不新建独立玩法系统 |
| `UI.Combat.DebuffTray` | 减益分区 | 复用同托盘，危险减益优先 |
| `UI.Combat.BuffIcon` | 单增益图标 | 叠层、来源、剩余时间与详情 |
| `UI.Combat.DebuffIcon` | 单减益图标 | 驱散类型、危险符号、剩余时间 |
| `UI.Combat.CrowdControlBadge` | 眩晕/沉默/击飞等控制提示 | 高优先级变体，关键控制不被普通溢出覆盖 |
| `UI.Combat.CrowdControlTimer` | 控制剩余时间 | 复用GamePlatformCountdownWidget（通用倒计时） |
| `UI.Combat.DispelIndicator` | 驱散类型与可驱散提示 | 取自授权战斗状态而非UI猜测 |
| `UI.Combat.ImmunityIndicator` | 无敌、免控、免打断提示 | 形状+文字+颜色联合表达 |
| `UI.Combat.TargetDebuffTray` | 目标减益栏 | 根据目标及施放者筛选、遵守战斗迷雾 |
| `UI.Combat.PartyAuraSummary` | 队友状态摘要 | Social/Arena（社交/竞技）消费同一个受控只读投影 |
| `UI.Combat.BossMechanicDebuff` | 首领关键机制状态 | 进入独立不可被普通增益挤掉的警告通道 |
| `UI.Combat.StatusEffectDetail` | 状态详情 | 复用GamePlatformTooltipWidget（通用悬浮提示） |
| `UI.Combat.EffectStacks` | 效果叠层 | 现有Stacks（层数）加额外阈值样式提案 |
| `UI.Combat.EffectExpiration` | 即将到期提醒 | 已授权时间锚点+共享UI显示调度 |

## 三、现有实际C++证据与缺口

已有文件：

- `GamePlatformUI/Source/GamePlatformUIClient/Public/Components/GamePlatformStatusEffectTrayWidget.h`（通用效果托盘头文件）。
- `FGamePlatformUIStatusEffect`（状态效果显示结构）：`EffectId`（效果标识）、`DisplayName`（本地化名称）、`Icon`（图标软引用）、`Stacks`（叠层数）、`bBeneficial`（有利状态标记）、`RemainingSeconds`（剩余秒数）。
- `FGamePlatformUIStatusEffectTrayState`（状态列表快照）：`OwnerDisplayId`（所有者标识）、`Revision`（修订号）和`Effects`（效果列表）。当前实现按Owner/Revision过滤旧更新，单次最多64条，同一EffectId不可重复，检查层数与持续时间的合法性。
- `UGamePlatformStatusEffectTrayWidget::ApplyEffects`（应用状态列表方法）已经提供Blueprint（蓝图）状态变化扩展点。**不代表实际GameplayEffect（玩法状态）业务事件已经接线，也不代表已存在目标Widget蓝图。**

初始规划的差异项（其中1、2及3中的多数显示字段、5中的分组排序/溢出已在当前C++实现，以下按设计历史保留）：

1. `bBeneficial`仅有正负布尔状态，无法表达Neutral（中性）、Conditional（条件触发）、CC（控制）等并行语义；建议新增兼容的显示分类，不修改旧序列化字段身份。
2. 现有`EffectId`按语义类型去重；同一效果由多施放者产生多个实例可能冲突。应区分`EffectId`（类型）与`EffectInstanceId`（实例身份），由战斗客户端适配器产生稳定、可释放身份。
3. 缺少`SourceDisplayId`（施放者来源）、`DispelCategory`（驱散类别）、`bLocallyDispellable`（本地玩家可驱散）、`MechanicTags`（效果机制标签）、`Importance`（显示重要性）、`VisibilityScope`（可见范围）、`bCritical`（关键机制）等字段。
4. 仅有`RemainingSeconds`快照，不适合在高延迟时逐条精确平滑更新。建议增加已校准时间锚点，只有共享UI显示时钟刷新倒计时，具体效果是否到期仍由玩法系统决定。
5. 现有64条属于安全容量上限，**不是屏幕可视数量**。应增加按区域展示数量、关键效果固定、优先级排序、稳定位置及`+N`（更多项目）折叠策略。
6. 玩家、目标、焦点、队友、Boss（首领）及竞技敌方的效果可见性必须分别受权限控制；UI不能直接遍历敌方ASC（能力系统组件）获取秘密信息。

## 四、推荐显示投影字段（仅设计，不宣称现有实现）

| 建议字段 | 中文语义 | 数据边界 |
| --- | --- | --- |
| `EffectInstanceId` | 效果实例身份 | 用于多来源、刷新和删除事件的精确匹配 |
| `EffectId` | 状态效果类型身份 | 保留既有语义与素材映射 |
| `SourceDisplayId` | 显示许可的施放者身份 | 未获权限不展示匿名或隐藏来源 |
| `Polarity` | 有利/不利/中性/条件性 | 与机制类型正交 |
| `MechanicTags` | 控制、DoT、HoT、护盾、免疫、触发等机制标签 | 不赋予权威Gameplay含义 |
| `DispelCategory` | 可驱散类别 | 来源战斗服务 |
| `bLocallyDispellable` | 本人此刻可驱散 | 由技能和战斗事实确认 |
| `Stacks` | 叠层数量 | 与既有字段兼容，业务明确0层语义 |
| `DurationSeconds` | 原始持续秒数 | 用于UI显示，不控制业务效果的结束 |
| `ExpirationTimeAnchor` | 已校准到期时间锚点 | 不依赖普通机器墙钟 |
| `Importance` | 状态展示优先级 | 由项目/竞技组合策略决定 |
| `VisibilityScope` | 玩家/目标/队友/首领可见性 | 经过服务端/复制层许可 |
| `bCritical` | 致命机制标志 | 不被常规状态溢出挤出关键区域 |
| `SortKey` | 稳定显示排序键 | 排序必须确定、图标不乱跳 |
| `Revision` | 递增来源版本 | 复用当前去重策略 |
| `SourceScopeId` | 数据来源/角色/世界代次 | 切地图和目标后拒绝旧事件 |

字段只是后续兼容性评审候选，不直接创建`BuffSubsystem`（增益子系统）和`DebuffSubsystem`（减益子系统）两套重复服务。

## 五、优先级与多视图

| 展示区域 | 展示重点 | 主要约束 |
| --- | --- | --- |
| 玩家本人 | 所有允许展示的Buff、Debuff与关键控制 | 不在UI里直接施加/解除效果 |
| 当前目标 | 重要状态、本人施加的减益 | 不能通过客户端显示敌方隐藏状态 |
| 焦点目标 | 高优先的目标关键效果 | 不重复维护第二份状态真源 |
| 队友框体 | 重要治疗/驱散/失控状态 | 不展示超出队伍规则的完整隐私数据 |
| 首领框体 | 阶段、免疫、危险机制减益 | 不提前泄漏服务器尚未公开的机制 |
| 竞技敌方 | 竞技规则允许展示的信息 | 保持迷雾、观战延迟和权限边界 |

**优先级建议：** CriticalMechanic（致命机制）→ HardCC（硬控制）→ ActionCritical（操作关键，例如可驱散或即将失效的防护）→ ShortTerm（短期战术）→ LongTerm（长期增强）→ Cosmetic（纯装饰）。允许项目/玩家设置变更非关键类别展示顺序，但不能把紧急预警悄悄隐藏。相同层级优先按剩余时间紧迫度、是否本人施放、稳定实例ID排序。

## 六、事件数据流及性能

```text
服务器 / GAS（技能效果权威事实）
  ↓ 经过观察者权限和网络复制筛选
CombatClientAdapter（战斗客户端适配）
  ├─ Applied（效果加入）
  ├─ StackChanged（层数变化）
  ├─ DurationRefreshed（持续时间刷新）
  ├─ Removed（效果移除）
  ├─ TargetChanged（观察目标变化）
  └─ Reconciled（网络恢复/预测修正）
  ↓ SourceScope/Generation/Revision（来源作用域/代次/修订检查）
StatusEffectViewModel（战斗只读显示投影，建议扩展现有接口）
  ↓ TypedDelegate（类型事件），仅广播真实变化
UGamePlatformStatusEffectTrayWidget（平台状态托盘）
  ├─ BuffTray（增益分组显示）
  ├─ DebuffTray（减益分组显示）
  ├─ CriticalInfo（致命机制独立区域）
  └─ Tooltip + Countdown（详情与通用倒计时）
  ↓ Monolith MCP 在UE编辑器中创建的项目Widget蓝图
DBAUIPack_Core（公共神兽UI资源） / DBAArena（竞技UI资源）
```

禁止逐Widget`NativeTick`（业务逐帧刷新）、`GetAllActorsOfClass`（业务遍历世界）、每个图标一个定时器。只有可见倒计时文本使用共享的UI刷新时钟；离开世界/注销/竞技结束须解绑委托、取消异步图标请求并释放自身租约。图标软引用、按需创建/池化、事件去重、可见列表裁剪的具体资源预算待实机测试确认。

## 七、拟制蓝图资源清单（均未验证存在）

```text
DBAUIPack_Core/Content/UI/Components/                   # 项目公共视觉内容包
  WBP_DBA_UI_StatusEffects.uasset                     # 已规划的状态效果总托盘，仍待真实制作
  WBP_DBA_UI_StatusEffectIcon.uasset                  # 单图标、层数与倒计时（待制作）
  WBP_DBA_UI_BuffTray.uasset                          # 增益图标分组（待制作）
  WBP_DBA_UI_DebuffTray.uasset                        # 减益图标分组（待制作）
  WBP_DBA_UI_CrowdControlAlert.uasset                 # 控制效果重要提示（待制作）
  WBP_DBA_UI_DispelBadge.uasset                       # 驱散类别/可驱散符号（待制作）
  WBP_DBA_UI_ImmunityBadge.uasset                     # 免疫提示（待制作）
  WBP_DBA_UI_EffectOverflow.uasset                    # 溢出+N展开（待制作）
DBAArena/Content/UI/HUD/                               # 竞技专有可选内容
  WBP_DBA_UI_ArenaHUD.uasset                          # 复用许可视图组合，不复制效果系统（待制作）
```

上述名字仅作目标资产规划。项目原有`StatusEffects.json`（布局JSON）并非真实蓝图。根据AGENTS.md（项目规则），所有项目视觉蓝图由Monolith MCP通过实际Unreal Editor（虚幻编辑器）创建、编译、保存和回读，文档或Python生成不得冒充.uasset。

## 八、验收案例

| 案例 | 验收要求 |
| --- | --- |
| 同类型Buff由两个施放者施放 | 允许按实例分开，不错误去重或泄漏施放者 |
| Debuff叠层更新、持续时间刷新 | 真实变更才刷新，没有旧图标残留 |
| 硬控、免控和可驱散同时存在 | 关键图标优先、类型说明不矛盾 |
| 永久状态与未知倒计时 | 使用∞/未知提示，不出现异常负数 |
| 64条同时生效、屏幕空间不足 | 视觉列表有界，关键效果优先，余项+N收纳 |
| 断线重连、换世界或高速切换目标 | 拒绝过期作用域和修订，不串号 |
| 隐身敌方、竞技迷雾或观战 | 只显示被授权的信息，不泄漏隐藏状态 |
| PC、手柄、移动设备和色觉模式 | 位置、焦点、文本和形状可识别，不仅靠红绿 |
| 图标异步加载失败 | 文字/基础符号安全回退，不影响战斗结果 |
| 1v1至5v5匹配 | 队伍/敌人可见性按正式竞技规则工作 |
| Unreal Editor / Monolith | 必须记录编译、保存、回读、PIE与Cook真实结果 |

**当前结论：** 游戏平台层状态效果C++托盘已具有独立Buff/Debuff/关键分组、驱散标签、控制分类、实例身份、重要性与溢出策略；完整AAA体验仍缺观察者真实战斗事实授权适配、共享计时展示调度以及Monolith实物蓝图和运行验证。新增施法条、目标框、关键警告的中立基础类已通过UE5.8定向编译，但视觉资产及综合战斗链路尚未交付。
