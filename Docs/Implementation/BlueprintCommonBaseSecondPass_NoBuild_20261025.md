# SSLM 蓝图及公共能力模块化第二轮审查与优化记录（2026-10-25，不编译）

> 工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（神兽联盟 UE5.8 主工程）  
> 分支基线：本轮开始为 `main/e5b2cfa6`（主分支版本），工程含并行 VFX 审核资源变更。  
> 用户门禁：本轮**不编译验证**；没有运行 UnrealBuildTool（虚幻构建工具）、Editor（编辑器）、Automation（自动化用例）、PIE（编辑器内运行）、Cook（资源烘焙）或联机验证。仅静态检查代码/离线索引、修改不涉及业务权威的公共类型和消费者，保留其他任务文件。

## 一、架构审查结论

当前项目**不是缺少公用组件体系**，而是部分跨领域的小型规则分别重复实现、少数协调类过大以及一部分蓝图真实父类/内容图尚缺原生证据。要坚持唯一机制和低层复用，但不要把无状态策略硬塞进 Actor/Subsystem（场景对象/子系统）继承树。

**已有可直接复用的公用基础：**

- PCG（程序化内容）：`AGamePlatformPCGActorBase`（公共PCG放置器父类）统一拥有图、Profile（生成配置）、Seed（种子）、来源ID和编辑器范围；Volume、Spline、Polygon、Connector（体积、样条、多边形、连接件）均为已有扩展。项目湖、树林、地块只应基于此类/其真正的子类和资产配置，不再重复生成框架。
- 水体：`UGamePlatformWaterInteractorComponent`（水域交互公共组件）在服务器低频采样离散状态，状态复制给客户端；水面材质/波纹属于客户端表现。项目湖和河流不得新增第二个水体权威采样器。
- 摄像机：`UGamePlatformCinematicCameraSubsystem`（平台电影摄像机子系统）已经处理当前本地玩家机位拥有权、跨世界清理、焦段/光圈和安全恢复；`UDivineBeastsCharacterPreviewSubsystem`（项目角色预览）已经调用它，项目层无需重新实现摄像机切换总线。
- 技能/战斗：`UGamePlatformAbilitySystemComponent`（技能系统组件）与 `UGamePlatformCombatComponent`（战斗组件）承担通用权威能力；`DivineBeastsV6GameplayAbility.cpp`（V6项目技能公共实现）已有集中原语。项目差异优先Definition（定义资产）和GAS（游戏技能系统）策略，不为每生肖/每技能单建Actor。
- VFX（视觉特效）：`GamePlatformVFXWorldSubsystem`（世界特效服务）已有 Resolution、Execution、Instances、Pooling、Scalability（解析/执行/实例/池化/伸缩）分工，项目表现只注册目录/提交中立请求，不能再建一套Niagara（粒子系统）播放器。
- SFX（声音效果）：`GamePlatformSFXWorldSubsystem`（世界声音服务）是声音执行拥有者；项目音效目录按资源包注册，失败不改变技能或命中。

## 二、本轮明确发现并优化的重复逻辑：逻辑定义转主资产身份

先前至少存在两份几乎等价的定义身份构造实现：`GamePlatformSFXPolicy.cpp`（音效策略）和 `GamePlatformVFXWorldSubsystem.cpp`（世界特效服务），两者都独立调用 `FGamePlatformId::TryParse`（逻辑编号解析），再构造相同的 `FPrimaryAssetId`（主资产身份）。项目表现逻辑预载也维护一次自己的等价转换。

现在向已有 `UGamePlatformPrimaryDataAsset`（平台主资产公共基类）新增唯一 `BuildDefinitionAssetId`（构造规范定义身份）入口，返回 `EGamePlatformDefinitionIdBuildStatus`（身份构造状态），包含合法、非法编号、空规范名三种明确结果。**失败时清空输出**，避免上一请求遗留的主资产ID。

已接线实际消费者：

1. `GamePlatformSFXPolicy.cpp`（音效策略）通过公共入口获得主资产身份；原有 `SFX.DefinitionIdInvalid` 错误码和两种中文提示仍按错误原因保留。
2. `GamePlatformVFXWorldSubsystem.cpp`（世界特效服务）保留原有内部布尔包装和全部既有调用点，仅将身份规范解析交给数据基类。
3. `DivineBeastsPresentationClientSubsystem.cpp`（项目表现协调器）逻辑预载循环使用公共主资产身份；去重仍在项目请求作用域内，Data（数据服务）仍是资源租约和加载唯一拥有者。

这次没有更换任一 `DefinitionId`（定义编号）、内容包路径、主资产类型、序列化字段、网络协议或句柄生成规则。项目天气初始预设的服务器装载链保持原状，避免在不编译情况下跨越服务器权威边界。

**新增待执行的原生测试源码：** `GamePlatformDefinitionAssetIdResolutionTests.cpp`（平台定义身份转换测试），覆盖规范小写/大写、文件路径伪造、空编号、版本前导零和失败后清空旧ID。本轮仅写入代码，**没有编译或运行测试，不宣称测试通过**。

## 三、蓝图资源静态复盘

新增 `Tests/Architecture/ReportReusableBlueprintCandidates.ps1`（可复用蓝图候选静态脚本），运行一次，不访问UE编辑器、不编译或保存资产。

生成报告：`Saved/Validation/BlueprintModularization/20261025/ReusableBlueprintCandidates.json`。

这次按 `BP_`、`WBP_`、`ABP_` 等常见命名前缀盘点136份 `.uasset`（UE二进制资产），离线索引结果为：

- 85个 `WidgetBlueprint`（界面控件蓝图）
- 43个 `Blueprint`（普通蓝图）
- 6个 `AnimBlueprint`（动画蓝图）
- 2个 `ObjectRedirector`（历史路径重定向器）

这136份是**按命名检索的候选集合，不是全工程真正蓝图类总量**；技能 GA/GE（玩法能力及效果蓝图）另有大量资产，不能因为未进入本次命名前缀列表而算作已审查完成。

离线摘要优先复核：

- **P1：`ABP_DBA_WorldLocomotion`（通用世界移动动画蓝图）**。离线摘要52节点，其中51个被归为非事件节点（索引时间2026-10-13）；核查动画层、共享状态机、骨骼兼容和角色复用，优先配置与Linked Anim Layers（链接动画层），不直接复制六份AnimGraph（动画图）。
- **P1：技能槽与状态条**。`WBP_DBA_UI_AbilityBar`（技能栏控件蓝图）离线有12个非事件节点，`WBP_DBA_UI_SkillBarMomentum`（气势条）10个，`WBP_DBA_UI_AbilitySlot`（技能槽）6个。先确认现有 `UGamePlatformSlotWidget`（平台通用槽位基类）继承、委托与展示职责，再决定具体Widget资产迁移。
- **P2：水体与环境对象**。`BP_DBA_VillageWaterfall`（新手村瀑布）和 `BP_DBA_ReviewLake/River_NoCarve`（水岸审查对象）离线各4个非事件节点。检查是否真正复用 `GamePlatformWater`（通用水体）组件与PCG公共Actor，而不是依据名称重建。
- **P2：背包/组队/登录大型界面**。离线索引中 `WBP_DBA_UI_Inventory`（背包）、`WBP_DBA_PartyPanel`（组队）、`WBP_DBA_UI_Login`（登录）的非事件节点摘要均为0；这**不等于复杂度低或无行为**，要结合其C++父类和实际Widget树才能判定。

特别说明：离线索引可能旧于磁盘文件；默认 `Event Tick`（逐帧事件）节点出现不证明有执行连线。没有真实父类、全部事件图节点连线、函数图、依赖消费者、资源编译/冷重载，因此没有删除、改名或迁移任何 `.uasset`、`.umap`（地图文件）。

## 四、仍需分阶段治理的职责集中问题

**P1 — 项目表现协调器：** `DivineBeastsPresentationClientSubsystem.cpp`（神兽联盟本地玩家表现协调器）约1099源码行，涵盖目录租约、天气、水体、音效、鸟群和自然落叶。应保持一个 LocalPlayer（本地玩家）组合入口，将各领域协调逻辑逐步下沉已有平台服务/模块私有Adapter（适配器）或策略；不能拆成一堆生命周期交叉的新子系统。

**P1 — 通用动画移动图：** `ABP_DBA_WorldLocomotion`（世界移动动画）优先进行原生图审计，按共同动画合同抽成可复用Linked Anim Layer（链接动画层）/共享状态机，再由人物或怪物蓝图组合；重载和骨骼兼容未证实前不修改动画资产。

**P2 — 世界天气与雪地审核：** `DivineBeastsWorldGameMode.cpp`（项目世界游戏模式）仍承载开发雪天审核排程，`DivineBeastsVillageSnowWorldSubsystem.cpp`（新手村雪地表现）同时负责材质、FootContact（脚步接触）、截图审核。未来应把仅开发审查用途从正式世界流程中解耦，但本轮不碰服务端定时器或天气状态复制。

**P2 — VFX协调器：** `GamePlatformVFXWorldSubsystem.cpp`（平台VFX世界服务）约1559源码行。已有私有Resolver、Executor、Registry（解析器/执行器/登记器）等领域组件，不宜另建世界服务；后续在可编译/性能验收阶段仅迁移可独立测试的私有策略和诊断，不变更系统作用域、实例句柄或资源租约。

**P2 — V6服务器技能：** `DivineBeastsV6GameplayAbility.cpp`（V6项目公共技能原语）约842源码行，内部含目标检查、命中/距离和伤害规格形成。已有多技能共享基础实现，但这些是服务器权威规则，未经联机和行为回归不应在本次“不编译验证”中改动，下一阶段先做函数职责图和可复用策略拆分设计。

## 五、执行边界与完成状态

本轮已完成真实源码检查、最小公用定义ID转换、三个已有调用方迁移、静态脚本编写与离线索引盘点。涉及的公共身份转换只属于Data（数据）的逻辑类型，不应被误实现为新UActorComponent（场景组件）。

- [x] 公共数据基类新增身份转换方法、保留稳定资产类型。
- [x] SFX、VFX、项目表现预载三个真实消费者已接线。
- [x] 新增用于后续运行的身份转换UE自动化测试源码。
- [x] 可复用蓝图候选离线盘点脚本已执行，得到136项资产分类与优先队列。
- [x] 已执行代码差异格式的只读检查，未发现补丁空白格式错误。
- [ ] **未编译**最新任何模块或目标；测试源码也**未运行**，无编译或功能通过结论。
- [ ] Monolith实时编辑器仍无已接入服务，真实蓝图父类、事件图、保存/冷重载与人工审核未执行。
- [ ] 角色/战斗、水体、PCG、摄像机、VFX、SFX的所有具体资产依赖尚未逐份得到原生引用证明。
- [ ] 新手村、OpenWorld（常驻世界）、MainArena（竞技场）的联机、Dedicated Server（专用服务器）剥离、Cook/Stage（资源烘焙/打包）未做。

**本轮没有改写业务权威时序、没有执行资产迁移、编译验证、重启编辑器、Git Commit/Push（提交/推送）。当前工作区中存在并行任务VFX的未提交二进制资源，本轮未触碰。**

**后续先决项：** 再取得Monolith真实资产图信息后，先验收通用动画蓝图及技能槽父类，再根据真实消费者迁移；对项目表现协调器和V6技能的大型类，必须在具备编译与行为回归条件后方可逐个实施有风险的功能拆分。
