# SSLM V6 GAS 技能相关文档与源码清单（2026-10-18）

**目录索引用途：** 给策划、美术、GAS研发、UE蓝图制作、QA、实施和技术负责人一个明确的唯一资料入口。路径从`DivineBeastsWorkspace`（SSLM工作空间）根目录起算。英文文件、类名旁提供中文职责说明。标记“已写源码”不代表已编译或可上线；不将不存在的.uasset列为完成。

| 编号 | 文件／目录 | 中文用途 | 本轮状态 | 后续门禁 |
|---|---|---|---|---|
| 01 | `Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.md` | V6历史传承八槽96技能详细中文设计 | 已存在，待人工审批 | 核对名称、机制、反制 |
| 02 | `Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json` | V6 12英雄／96技能唯一设计真源，保留历史原名和V5映射 | 已存在，未批准投产 | 逐技能签审 |
| 03 | `Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.csv` | 96行策划审核及一级候选数值对照 | 已存在 | 技术/策划会签 |
| 04 | `Docs/Design/ApprovedSkillDisplayNames_V5.json` | 已获用户采用的V5稳定显示名 | 保留，V6不覆盖 | V6名称裁定再迁移 |
| 05 | `Docs/Design/ZodiacEightSlotSkills_V2.json` | 旧八槽一级数据候选和基础动作字段 | 保留，非V6最终平衡 | 转换为V6正式平衡表 |
| 06 | `Docs/Design/ZodiacEightSlotAbilitySetManifest_V2.json` | 12英雄×8数据授权参考清单 | 旧占位参考 | V6逐英雄重新审核 |
| 07 | `Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json` | 96个技能逐项实施状态、模式候选与未制作资产指针 | **新建，96项全部禁止授予** | 逐项填入原生GA/GE/VFX与验收信息 |
| 08 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Public/Abilities/DivineBeastsV6GameplayAbility.h` | V6共享原生GAS技能执行接口、8种安全模式 | **新建C++，未编译** | UHT/UBT和UE原生用例 |
| 09 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Abilities/DivineBeastsV6GameplayAbility.cpp` | 服务器权威单体／范围／投射、治疗、护盾、控制、简短位移 | **新建C++，未编译** | 真实伤害/目标/取消/同步重入测试 |
| 10 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Public/Definitions/DivineBeastsAbilityDefinition.h` | 项目DataAsset身份、V6原生审核位；保留旧草案禁用位 | **本轮已改，未编译** | UHT原生反射回读 |
| 11 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Definitions/DivineBeastsAbilityDefinition.cpp` | V6原生审核位与草案位互斥、定义数值校验 | **本轮已改，未编译** | DataValidation引擎运行 |
| 12 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp` | 服务器装配前V6 GA类／Definition／GE核验与回滚 | **本轮已改，未编译** | 真实ASC同步通知及冷重载 |
| 13 | `Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/DivineBeastsAbilitiesRuntime.Build.cs` | 项目技能模块公开依赖及导航私有依赖 | **本轮已改，未编译** | UE5.8增量构建 |
| 14 | `Tests/Architecture/TestV6GASImplementation.py` | 源码及96项台账静态正反向安全门禁 | **新建，须执行** | 真实退出0方可标记通过 |
| 15 | `Tools/Unreal/Abilities/AuthorZodiacEightSlotDesignAssets.py` | 已有V2数据资产编辑器作者脚本 | 保留，不等于V6已导入 | Monolith/原生编辑器独占作者窗口 |
| 16 | `Docs/Design/ZodiacSkillV3Review/CharacterCreatePreviewContract.json` | 角色创建8技能图文、固有被动及效果图权限合同 | 已存在，正式页面未接入 | UI蓝图与GamePlatformData回读 |
| 17 | `Tools/Unreal/UI/EightSlotAbilityBarWidgetSpec_V2.json` | 战斗8槽WBP命名和布局装配规格 | 已存在，部分Widget待绑定 | Monolith真实保存与视觉验收 |
| 18 | `Docs/Implementation/GamePlatformAbilitySystem/GASReentrancyRemediation_20261018.md` | 旧GAS授权/撤销重入修复及测试证据 | 已有记录，不等于V6已编译 | 新旧方案联测 |
| 19 | `Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementation_20261018.md` | 本次GAS服务器实现范围、能力矩阵、开发阻断及验收 | **新建实施说明** | 签收可执行范围 |
| 20 | `Docs/Implementation/GamePlatformAbilitySystem/V6SkillDocumentIndex_20261018.md` | **本文件，统一文档清单索引** | 新建 | 后续每批改动同步维护 |

## 需要原生创建而当前不存在的资产台账

| 资产类别 | 目标数量 | 是否已生成 | 禁止伪造的证据 |
|---|---:|---|---|
| V6技能Definition数据资产 | 96 | **未生成** | AssetRegistry身份、字段回读、冷重载 |
| V6一级／多级正式平衡DataTable | 至少1 | **未生成** | 主资产行、已批准数值 |
| 12英雄V6技能AbilitySet数据资产 | 12 | **未生成** | 8GA/8输入标签、共享GE身份 |
| 正式GA蓝图和必要专用权威行为 | 96 | **未生成** | 真实可激活、角色/服务器运行结果 |
| 技能成本与冷却GE | 按真实技能配方 | **未生成** | 动态成本负值及冷却标签同级别校验 |
| 96项48×48技能图标 | 96 | **未全部生成并审核** | 原生Texture2D身份、用户人工签批 |
| 单技能VFX/Niagara/GameplayCue | 96套候选 | **未生成全套正式资源** | 45°游戏视角、分镜、VFX预算和场景测试 |
| 创建角色8技能详细图文卡+固有被动 | 12×8加12被动 | **未完成正式Widget绑定** | 页面身份、图像哈希、换英雄无旧回弹 |

## P0验证清单

先完成V6技能名称／机制签审，建立原生DataAsset；每个技能审核`AbilityId`、英雄身份、8槽输入、GAS执行模式、Cost/Cooldown实际GE、ServerOnly/预测策略、施法/命中/取消、VFX语义及角色创建展示。不能只靠96项静态JSON判断已实现96个技能。

**实施规则：** 本批不编译，不写任何虚假.uasset，不修改已运行旧占位Hero默认技能集，不恢复旧五行、王印、三合六合六冲及共鸣玩法，不自动提交Git。
