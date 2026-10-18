# 十二生肖八槽草案资产制作链路实施记录（2026-10-18）

## 检查基线与并行成果

核对仓库 `freebooz/DivineBeastsWorkspace` 的 `main` 分支，当前基线提交为 `f8736343554a103167d491169f74d4e720b278e6`。该提交已包含十二生肖八槽设计JSON、UE作者脚本、数值CSV、C++定义及原有静态测试。按 `zodiac`、`eight`、`ability` 关键词检查相关分支及未合并拉取请求，未发现另一条并行实现；本次在既有链路上增量修补，不再重复生成原有96项设计或重做旧的五槽占位资产。

现状仍是：12名英雄各8项设计，共96项；设计状态为 `PENDING_HUMAN_BALANCE_REVIEW`（待人工数值平衡审核），原生资产状态为 `NOT_AUTHORED`（尚未制作）。原作者脚本没有读取配套CSV作为数值行真源，也未完整保留行为字段；脚本在创建对象后才判断目标路径是否已存在，且没有覆盖所有目标资产的写入前所有权预检。C++设计行为结构仅能存部分通用数值，输入动作、默认键位、显示序号、多个表现语义以及完整行为源字段都没有对应反射属性。

## 本次改动

- 新增 `Tools/Unreal/Abilities/ZodiacEightSlotDraftContract.py`（八槽数据契约转换器）。该文件只使用Python标准库，可在不导入Unreal的情况下校验96行CSV与JSON的技能身份、等级、类型、数值、输入标签、槽位顺序、行为字段、状态及资源边界，并生成确定性的资产路径和源SHA-256摘要。未知或缺失行为字段、数值漂移、重复行名、跨英雄身份、未审批授权、表现硬引用或输入语义变化均失败关闭。
- 扩展 `FDivineBeastsEightSlotDraftAction`（八槽设计行为结构）和 `UDivineBeastsAbilityDefinition`（技能数据定义）：行为保留明确的数值单位、伤害类型、服务器导航校验、复活标志和原始行为JSON；定义新增输入动作、默认键位、显示序号、数据归属、目录身份、SHA-256源摘要、VFX语义键及完整表现阶段列表。C++ `ValidateDefinition`（定义验证）同步检查必要字段和语义组合。
- 重写 `AuthorZodiacEightSlotDesignAssets.py`（UE编辑器资产作者脚本）的映射流程。作者脚本从纯Python契约读取CSV和JSON；进入写入前先校验UE5.8版本、正式工程、明确制作开关、全部反射字段、十二个现有英雄定义和占位集合身份，并预检目标表与96个DataAsset（数据资产）。非本插件隔离目录、错误类型、非草案资源、不同目录所有者、英雄/槽位/逻辑ID冲突或已变化的正式占位集合均拒绝覆盖。只在 `/DBAGameplay/Abilities/EightSlotDrafts`（项目技能插件隔离草案目录）写入；不写入Hero的 `DefaultAbilitySetId`（默认技能集合标识），不创建GA（玩法能力）、动画、Niagara（粒子特效）或图标资产。
- 生成 `Docs/Implementation/ZodiacEightSlotDraftAssetPlan_V2.json`（八槽原生资产路径计划）。该文件列明每名生肖的8个DataAsset对象路径、技能稳定ID、等级行名、输入标签/动作、键位与源摘要；它是路径映射，不是资产创建证据。计划明确记录96份DataAsset、96行数值表尚未制作，正式集合尚未分配，技能未获GAS（玩法能力系统）授权，动画、VFX和图标未创建/绑定。
- 更新 `Tests/Architecture/TestZodiacEightSlotDesign.py`（八槽技能静态合同门禁），覆盖CSV漂移、行为字段未知、输入槽错配、行为逐字段保留、资产归属/路径、源摘要和路径计划再生成一致性等负例与正例。

## 已完成的静态验证

以下检查针对从上述主分支读取的项目文件快照执行：

- `python3 -m py_compile Tools/Unreal/Abilities/ZodiacEightSlotDraftContract.py Tools/Unreal/Abilities/AuthorZodiacEightSlotDesignAssets.py Tests/Architecture/TestZodiacEightSlotDesign.py`：退出码0，三个Python文件语法通过。
- `python3 Tests/Architecture/TestZodiacEightSlotDesign.py`：14/14项通过，包含96项设计与CSV一致、旧输入槽合同、草案授权失败关闭及新增所有权/幂等/映射回归。
- 纯Python路径计划生成器：退出码0，确认12名英雄、96项唯一技能ID、96行候选数值、96个隔离对象路径。
- 未运行UHT、UBT、MSVC、蓝图编译或Cook（烘焙），符合当前集中编译验证的要求。以上结果只证明源码静态合同和Python工具检查通过，不代表C++构建或引擎资产检查通过。

## 原生资产与正式启用状态

本次执行环境只有GitHub仓库访问能力，没有暴露UE5.8编辑器或Monolith MCP（编辑器资产自动化接口），因此没有调用 `author()`、保存资产或进行独立冷重载。实际创建数量为：DataAsset 0/96，DataTable（数据表）0/1。计划文件中的路径均为预期映射，不得计为实物资产。

所有正式生肖技能的旧占位技能集合保持不变。96项草案仍处于待审批状态；脚本不会将它们添加到默认技能集合，也不会把VFX语义标识伪装成实际Niagara引用。

## 正式启用前审批与验收

1. 策划人员逐英雄审核八项技能的数值、目标、控制/治疗/移动/防御行为、键位和描述，明确通过版本并完成平衡审批。
2. 玩法与技术负责人确认96项行为已落实为服务器权威GAS执行，并核验动画、VFX、图标和客户端八槽界面的真实资源与回退策略；草案DataAsset本身不授予技能。
3. 在加载了新增C++反射字段的锁定UE5.8编辑器中，通过Monolith MCP显式运行作者脚本；检查真实表行、96个DataAsset字段和保存结果，再从独立编辑器进程冷重载并运行原生DataValidation（数据校验）。
4. 仅在逐英雄审批、服务端授权、客户端输入/显示、Dedicated Server与Client构建、Cook资源隔离和联机验收完成后，另行审查是否更新对应英雄的正式 `DefaultAbilitySetId`。本记录没有批准该切换。


## 续作：96项中文名称、详细说明和技能设计提示词

### 基线与并行工作整合

本轮已核对主分支`236bb26dd60b0a9f1ad48de1419442eb25fe44b5`（最新源快照），并保留该提交的技能集合清单、八槽界面规格、原生1—8输入转发、12项控制伤害类型修正、4项混合范围目标修正及全部新增测试。分支通过双亲提交整合该基线，不改写并行成果。

### 中文文本合同与内容

- 设计目录版本为2.1.0。96项稳定技能编号及中文名保留，`descriptionZh`（中文技能说明）按既有动作及一级候选值重整；新增`designPromptZh`（中文效果概念图提示词），逐项编写主形、阶段、目标、可读性与禁止扩张机制的要求。
- 提示词包含当前英雄及技能中文名，引用唯一全局V3.0配色方向和现行技能特效规范；不引入五行战斗、破元、共鸣等已取消机制。文字与视觉内容状态仍为`PENDING_HUMAN_TEXT_AND_VISUAL_REVIEW`（待人工文本与视觉审核）。
- 把寅虎“威仪昂扬”、申猴“灵猴戏法”、亥猪“玄鬃怒吼”的`intentTarget`（目标模式）从误标敌方修正为自身；不新增行为，不改变任何技能编号、名称、伤害、命中段数、气势、冷却、范围或动作数组。中文说明不再把无对应控制行为的“云鳞飞刃”写成冻结。
- `DraftDisplayNameZh`/`DraftDescriptionZh`（草案中文名称/说明）保留FText本地化数据；新增`DraftDesignPromptZh`（草案中文设计提示词）是`WITH_EDITORONLY_DATA`内的FString字段，只供编辑器保存/审核，Cook时剥离。旧正式占位定义仍不受草案验证分支影响。
- 作者启动前检查新增反射字段，旧DLL缺字段则在任何写入前停止；三项中文文本通过契约映射、制作后回读和源摘要一起校验。中文文案是否已在真实角色创建Widget显示，仍需另行通过Monolith与客户端验收，不能由此文本更新证明。
- 清单Schema升级为2：`assetPackagePath`（资产包路径）保留原目录与对象名，`assetObjectPath`（完整对象路径）补足“.对象名”；共享表同样区分包/对象路径。UE的GetPathName返回完整路径，不能直接与包路径比较。依据[UE5.8官方GetPathName文档](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/UObjectBaseUtility/GetPathName)。不发生资源移动或重命名。
- [中文技能审核索引](../Design/ZodiacEightSlotSkillTextReview_V2.md)与[资产规划清单](ZodiacEightSlotDraftAssetPlan_V2.json)均由同一校验器生成，覆盖12英雄、96项中文名称/说明/提示词及96行数值。

### 实际验证证据与范围

本地为从锁定GitHub提交提取的受影响文件快照，不是完整仓库检出。整合并行工作后的基线19项通过；新增文本/目标/路径回归先观察到预期失败，再完成实现。最终命令：

```bash
PYTHONDONTWRITEBYTECODE=1 python3 Tests/Architecture/TestZodiacEightSlotDesign.py
python3 Tools/Unreal/Abilities/ZodiacEightSlotDraftContract.py --catalog Docs/Design/ZodiacEightSlotSkills_V2.json --balance-csv Game/Plugins/DivineBeasts/DBAGameplay/SourceArt/Abilities/DT_DBA_ZodiacEightSlotDraftBalance_V2.csv --output Docs/Implementation/ZodiacEightSlotDraftAssetPlan_V2.json --review-output Docs/Design/ZodiacEightSlotSkillTextReview_V2.md
```

- 回归实际退出码0，`Ran 26 tests ... OK`；包括空白/缺失/纯英文占位拒绝、串技能提示词拒绝、文本哈希变更、三项自身增益、审核文档同源以及包/对象路径区分。
- 三个Python文件通过AST（抽象语法树）语法检查；源数据逐字段比较确认数值、动作数组与中文名称未改变，只有说明/提示词及上述3项目标修正。
- C++/UHT/UBT、蓝图、Cook、真实编辑器属性序列化与独立冷重载均未执行；当前无可调用的UE5.8/Monolith编辑器能力。新增编辑器字段的实际反射加载与Cook剥离仍需在最后统一验证。
- 本批新增图片0，原生DataAsset 0/96，DataTable 0/1；未修改任何英雄DefaultAbilitySetId，未解除草案禁用，未授予真实GAS，未伪造动画/VFX/图标。

### 人工审核与正式启用前门槛

逐技能审核中文名称、说明与概念提示词的准确性；持续伤害系数的计量基数/跳频、增益叠加、目标边界与全部一级数值尚须策划批准。随后在允许的统一编译批次加载UE5.8新增反射，经Monolith真实制作、保存及独立冷读97项资源，再完成对应GA/GE、动画/特效、角色创建界面展示、联机/性能与正式技能集合切换审批。确认实施方案不等于批准这96项内容投产。

## 最新续作：完整96项资产字段与本地制作条件（V2.2）

本轮核对主分支`4dde0b951ab3e5459f0f25f7f8ff483ae3a53ca0`，保留其V3历史名称审稿与其他并行成果；本作者仍明确以V2稳定技能编号和V2.2源数据制作隔离草案，不自动应用尚未确认的V3展示名称。

### 完整资产制作数据

- 每项均有唯一技能编号、中文名称/说明/设计提示词、一级基础伤害/类型/最多命中段数、施法准备时间、施法距离、冷却、区域半径、气势费用、输入槽位、数值行和占位信息。规划清单直接保存完整balanceRow（数值行）及placeholders（占位字段），可逐项审阅。
- 96个castTimeSeconds（施法准备秒数）候选为0—1.1秒；0为瞬发，最大允许30秒，定义为施法准入至第一次释放的准备时间。不是全段动画、飞行或引导时长。仍属LEVEL1_CANDIDATE_NOT_BALANCED（未平衡一级候选），不自动实现GAS时序。
- CastTimeSeconds进入原生数值行，默认0兼容旧行。只修改本轮草案CSV；既有正式占位技能数值资产未写入。
- 图标/VFX的占位身份分别为SSLM.Placeholder.Icon.Unbound（未绑定图标）及SSLM.Placeholder.VFX.Unbound（未绑定视觉特效），bDraftPresentationUnbound保持true。所有真实图标/Niagara引用仍为空；这些身份不是已生成视觉资源。
- 作者预检新增占位字段与数值行CastTimeSeconds反射；填表后导出JSON，逐行检查全部字段与96个行身份，拒绝字段丢失、时间变零、错行/重复。数值浮点回读允许float32误差。数据表导出API依据[UE官方DataTableFunctionLibrary文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/DataTableFunctionLibrary?application_version=5.4)，作者仍要求实际UE5.8且检查该接口存在，未声称本轮已执行导出。

### 实际本地连接证据与阻塞

本轮已通过Cwebcodex（本地执行连接器）连接固定DivineBeastsWorkspace项目。Monolith注册可解析，网关状态healthy仅说明保留的子进程还在；对monolith_status的真实调用返回`Monolith MCP is not available (Unreal Editor not running)`（编辑器未运行），因此没有取得引擎版本/当前工程/反射加载证明。此状态更新替代前节仅按工具暴露得出的“接口不可用”判断。

本地主工作区在main有3个合并冲突、14个活跃或排队任务，且读取到的技能定义头仍为旧53行版本。本轮没有修改该工作树或停止其他任务；本轮唯一的git log诊断作业在尚未开始执行时已停止。新增字段尚未合入、未统一编译，不能用旧模块生成不完整资产。所有代码/数据改动在现有GitHub隔离分支中完成。

### 验证与实际制作结果

在锁定主分支/草稿分支提取的受影响源快照中运行：

```bash
PYTHONDONTWRITEBYTECODE=1 python3 Tests/Architecture/TestZodiacEightSlotDesign.py -q
PYTHONDONTWRITEBYTECODE=1 python3 Tests/Architecture/TestZodiacSkillV3Heritage.py -q
```

- 实际退出码均0：31项八槽数据/作者门禁、10项V3历史继承兼容门禁通过。
- 施法时间缺失、负数、非有限数、布尔、超过30秒均拒绝；JSON/CSV/完整资产清单一致；占位身份丢失或虚构资源引用拒绝。原生导出校验逻辑的输入丢列、错值、缺行/重复行负向用例通过；输入为测试数据，不冒充真实UE导出。
- 三个受影响Python文件通过AST语法检查。本轮未执行C++/UHT/UBT、蓝图编译、Cook或完整工程回归；V3审稿文件与原数值均保留。
- 实际原生DataAsset创建0/96、DataTable创建0/1；保存0、独立冷重载0。当前交付是可核验的完整制作源、作者脚本与静态门禁，不能将JSON/CSV清单记为已生成.uasset。
- 尚须在最后统一编译并合入新增反射，启动锁定UE5.8主工程，经Monolith验证工程/版本/显式开关后运行作者、保存、独立冷重载；再经文本/数值/图标与特效/真实GAS/联机审核批准正式启用。现有正式占位集合继续保留。
