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

