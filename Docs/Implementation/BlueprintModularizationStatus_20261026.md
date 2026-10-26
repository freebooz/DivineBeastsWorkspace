# SSLM 蓝图模块化与公共组件复用状态报告（2026-10-26）

> 工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（神兽联盟唯一正式工程）  
> 版本基线：2026-10-26 开始时 `main/e5b2cfa6`。工作区同时包含其他任务的 VFX（二进制视觉特效）资产/源码改动，**本轮没有覆盖、清理或提交它们**。  
> 本轮执行限制继承用户上一条要求：**不编译、不运行自动化、不 PIE（编辑器内运行）、不 Cook（烘焙）**。源码静态差异检查与只读离线索引审查允许执行，但不构成引擎运行验收。
>
> 本文记录本轮源码已落盘与客观阻断，不能被解读为整项目已经完成 UE5.8 资产级验收；唯一长期继承/复用规范仍为 [《三层类继承与扩展规范》](../Architecture/三层类继承与扩展规范.md)。

## 1. 已完成的实际源码优化

### 1.1 平台公共 UI 快照校验收敛

在现有 `FGamePlatformUISnapshotScope`（平台界面来源快照校验策略）上复用两种不会持有世界对象的值校验：

- `CanAccept`（全局唯一来源 ID + 严格递增修订号），保留原有绑定作用域；来源必须一致、修订必须非负且更新。
- `CanAcceptNamedRevision`（稳定名称 ID + 严格递增修订号），保留原有无 GUID（全局唯一标识）场景的兼容行为；资源身份改变可从修订0重新开始，相同资源不能回退或重复修订。

已把逐个 Widget（界面控件）内的重复校验迁移到公共策略：

| 源组件 | 通用校验 | 功能与业务契约 |
|---|---|---|
| `GamePlatformCastProgressWidget`（施法条） | 来源 GUID | 原有时间窗、重入栅栏、控件动画保持 |
| `GamePlatformTargetFrameWidget`（目标框） | 来源 GUID | 迷雾/隐藏目标仍清空肖像和资源 |
| `GamePlatformStatusEffectTrayWidget`（状态效果栏） | 来源 GUID | 显式绑定使用公共校验，历史无绑定模式保留旧行为 |
| `GamePlatformMinimapWidget`（小地图） | 名称 + 修订 | 地图坐标和标记数据校验保持 |
| `GamePlatformQuestTrackerWidget`（任务追踪） | 名称 + 修订 | 目标列表容量和进度校验保持 |
| `GamePlatformCountdownWidget`（倒计时） | 名称 + 修订 | 时间非有限值、上限与显示格式保持 |
| `GamePlatformSlotBarWidget`（槽位栏） | 名称 + 修订 | 最多32格、重复槽位和遮罩范围仍校验 |
| `GamePlatformInteractionPromptWidget`（交互提示） | 名称 + 修订 | 只更新当前只读提示 |
| `GamePlatformSettingRowWidget`（设置项） | 名称 + 修订 | 沿用原有设置身份和修订策略 |
| `GamePlatformTooltipWidget`（悬浮提示） | 名称 + 修订 | 沿用原有内容身份和清空策略 |

项目/平台其他已接入的消费者：`GamePlatformPartyRosterWidget`（组队列表）、`GamePlatformCombatAlertWidget`（战斗警告）、`DivineBeastsScopedDomainViewModelBase`（项目领域视图模型），未增建平行管理器或子系统。

**保留内容**：所有 UCLASS（反射类）身份、蓝图父类、公开函数、Widget 命名、数据库/服务器业务真源不变。配套 `GamePlatformUISlotCommonPresentationTests.cpp`（平台快照原生测试源码）新增名称修订边界断言；源码已写，**未编译或执行**。

### 1.2 根布局四世界体验槽位策略解耦

新增项目私有 `FDivineBeastsWorldHUDRoute`（世界 HUD 路由策略）及实现文件，`DivineBeastsRootLayout.cpp`（项目根布局）改为调用其 `IsSupportedWorldRole` 和 `ResolveSwitcherIndex`：

- 0 = OpenWorld（常驻世界）
- 1 = Village（新手村）
- 2 = Village.Tutorial（教学体验）
- 3 = Village.Training（训练体验）

`MainArena`（竞技场）和未知角色不进入普通世界 HUD。保持此前“体验覆盖默认世界槽位”的优先级、作者 WidgetSwitcher（控件切换器）顺序、RootLayout 生命周期和项目世界身份，不变更真实资源。新增 `DivineBeastsWorldHUDRouteTests.cpp`（世界 HUD 路由自动化测试源码），同样未执行。

### 1.3 背包状态与错误提示项目化目录

`UDivineBeastsInventoryScreen`（背包页面）之前同时负责命令、事件、格子绘制以及一大段机器状态/错误码中文提示分支。现在增加纯项目表现 `FDivineBeastsInventoryTextCatalog`（背包本地化信息目录），统一 `ForState`（状态文本）和 `ForError`（错误文本），页面两个对外 Blueprint（蓝图）访问接口只转发平台客户端状态和错误码。

- 保留所有原有 `LOCTEXT_NAMESPACE="DivineBeastsInventoryScreen"`（翻译命名空间）和准确的翻译键、中文默认文本，没有改变原来的 UI 资产、对外函数或业务逻辑。
- 只映射平台稳定错误枚举，未知枚举使用已批准的兜底文本；不会把后端自然语言或账户信息带入 Widget（控件）。
- 新增 `DivineBeastsInventoryTextCatalogTests.cpp`（本地化目录测试源码），覆盖加载、错误、重复操作、结果未知和未知状态，但本轮不编译、不运行。
- 因为该中文表达属于项目品牌和游戏物品领域，没有将其塞进 `GamePlatformInventory`（平台背包）公共定义；平台继续唯一拥有业务错误码和库存服务。
- 已以当前主分支 `HEAD` 的旧页面代码为基线，对新旧 `LOCTEXT`（本地化文字）逐键比较：原始30个、迁移后30个，翻译键与中文默认文本差异0。这是静态文本一致性检查，不是UE运行验证。

### 1.4 既有三层公共结构继续沿用

保留已有的 `UGamePlatformSlotWidget`（平台槽位控件）、`UGamePlatformViewModelBase`（通用视图模型）、`UGamePlatformPrimaryDataAsset`（平台主资产基类）、`AGamePlatformPCGActorBase`（程序化内容对象基类）、`UGamePlatformWaterInteractorComponent`（水域权威交互组件）、`UGamePlatformCinematicCameraSubsystem`（平台电影摄像机）、VFX/SFX（视觉/声音）唯一执行服务等现有公共类型。此前已下沉的 `BuildDefinitionAssetId`（定义编号转主资产身份）由SFX、VFX和项目表现预载共同使用，本轮未改变它们的业务逻辑。

不因为某个类文件超过数百行就引入无真实消费者的 Manager（管理器）、第二个场景服务或三层空派生。尚未有静态可证明的父类替换安全方案时，不强制更换现有 UObject 父类。

## 2. 全部已索引蓝图离线审查结果

采用只读 Monolith 项目索引和工程真实磁盘路径生成以下文件：

- `Tests/Architecture/ReportAllBlueprintAssetsOffline.ps1`（完整蓝图离线类别与节点摘要盘点）
- `Saved/Validation/BlueprintModularization/20261026/BlueprintAssetClassInventory_Offline.json`（结构化离线清单）
- `Saved/Validation/BlueprintModularization/20261026/BlueprintAssetClassInventory_Offline.csv`（可按类、包、节点过滤的表格）
- `Tests/Architecture/ReportBlueprintOfflineDependencies.ps1`（离线蓝图依赖查找）
- `Saved/Validation/BlueprintModularization/20261026/BlueprintReferenceInventory_Offline.json`（依赖台账）
- `Saved/Validation/BlueprintModularization/20261026/BlueprintReferenceInventory_Offline.csv`（依赖表格）

| 离线索引资产类别 | 数量 | 已找到磁盘文件 | 原生父类和事件图已核实 |
|---|---:|---:|---|
| GA_（技能蓝图） | 205 | 205 | 0 |
| GE_（技能效果蓝图） | 252 | 252 | 0 |
| WBP_（界面控件蓝图） | 85 | 85 | 0 |
| BP_（普通蓝图） | 43 | 43 | 0 |
| ABP_（动画蓝图） | 6 | 6 | 0 |
| **合计** | **591** | **591** | **0** |

上述“原生父类和事件图已核实=0”表示**本轮没有使用运行中 UE 编辑器取得当前原生验证**，不等于从未存在历史编译结果。全部591份离线索引记录均能在当前磁盘找到对应路径，详情类型查询0异常；项目资产总体库存另包含材质、纹理、Niagara、地图等非蓝图类型，不能从上述591推断全工程资产总量或编译状态。

逐项离线引用查询：**591/591完成、0查询异常、仅12份资产有可检索的入站/出站索引关系**。这正好暴露出离线依赖索引的覆盖局限：大多数普通技能/效果、角色/世界内容的软路径、字符串动态加载与地图实例引用无法由“无索引引用”判定不存在。**绝对禁止把剩余579份蓝图当作可删除孤儿资产**。

### 2.1 优先复核清单

1. **P1 `ABP_DBA_WorldLocomotion`（通用移动动画蓝图）**：离线有52节点记录、其中51个被归为非事件节点。需要完整读取动画状态机、骨骼/IK（反向运动学）、动画图/链接层和角色使用者；按共同动画契约考虑Linked Anim Layers（链接动画层），不复制并替换所有角色的旧动画蓝图。
2. **P1 `WBP_DBA_UI_AbilityBar`、`WBP_DBA_UI_AbilitySlot`、`WBP_DBA_UI_SkillBarMomentum`**（技能栏、技能槽、气势条）：离线各有12、6、10个非事件节点摘要；正式技能栏对 `/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot`（战斗技能槽）存在已知硬依赖。另一个 `UI/Components/WBP_DBA_UI_AbilitySlot`（历史通用技能槽）仍须保留，直到真实父类、动态引用和视觉行为全部核实。
3. **P1 `WBP_DBA_UI_Inventory`、`WBP_DBA_UI_RootLayout`**（背包与根布局）：主要复杂度可能在已有C++父类/运行委托而不是离线EventGraph摘要。背包已提取公共格子展示，根布局已提取世界HUD路由；仍须在真实Widget树复核。
4. **P2 新手村PCG、水体及环境蓝图**：以前发现四组是旧路径 `ObjectRedirector`（重定向器）+ 正式资源，并非四份并列实现。真正的水体/植被资产应优先依赖现有平台水体组件与PCGActor原语；不在没有真实图审查的情况下改继承或删除。
5. **P2 205 GA + 252 GE（技能/效果）**：这些虽使用统一命名前缀，但没有真实父类、行为连线证据，不能声称所有技能均已模块化，或按文件数认定存在重复实现。优先抽查12生肖普通技、冷却、成本与同一种通用GAS父类/数据驱动的实际继承关系。

## 3. 静态检查与工程保护

- 扩充 `Tests/Architecture/ValidateSharedUIComponentReuse.ps1`（公共UI组件复用门禁），覆盖平台视觉值、来源GUID、资源名修订、世界HUD路线与原生测试源码，**静态32项检查通过**。
- 新增 `Tests/Architecture/ValidateSharedDefinitionIdReuse.ps1`（公共定义编号转换复用门禁），核对数据基类、SFX/VFX与项目三真实消费者、失败清空、现有错误码及测试源码，**静态11项检查通过**。
- 既有 `Tests/Architecture/ValidateInheritanceBoundaries.ps1`（三层继承边界源码扫描）已执行只读静态检查：587份公开头文件、1,278个类型、212条继承边通过；不包括蓝图二进制父类验证。
- 本轮仅执行静态 `git diff --check`（改动差异格式核查）和只读 PowerShell（脚本）资产审计；不执行 C++ 编译、UE原生自动化、PIE、客户端启动、服务器、Cook/Stage 或 Monolith 保存。
- HWebCodex 当前会话 `mcp_tool(action=list)` 返回可调用服务列表为空，无法对 `.uasset` 的 Widget/Anim Blueprint（控件/动画蓝图）执行受项目规则允许的真实修改。已存在的UE资产保持原字节，不通过脚本伪造或覆盖。
- 工作区另有并行VFX任务和脏资产，均未触碰。未运行 `git add`、`git commit` 或 `git push`（版本暂存、提交或推送）。

## 4. 实际完成边界与剩余阻断

**本轮静态源码整改与离线盘点工作已经落盘。全项目模块化发布验收尚未完成，且在“无编译/无Monolith实时编辑器”条件下无法宣称全部完成。**

| 验收项目 | 本轮状态 | 完成所需条件 |
|---|---|---|
| 源码公共状态/路由抽离 | 代码已修改，未编译 | 用户允许后完成受影响模块编译及行为回归 |
| 591已索引蓝图类型/磁盘对应 | 离线清单完成 | 新增/动态资产需在编辑器中再核查 |
| 591已索引蓝图离线依赖 | 离线查询完成，索引不完整 | UE AssetRegistry+Monolith读取当前硬软依赖和动态消费者 |
| 591蓝图原生父类/图连线 | 本轮无原生证据 | 连接Monolith实时MCP，逐包读取父类与Graph |
| 真实Widget/Anim资产改父类 | 未执行；规则不允许离线替代 | Monolith原生编辑、编译、保存、冷重载及人工审核 |
| Client/Editor/Server（客户端/编辑器/专服）构建 | 用户本轮禁止 | 后续明确允许进行编译验证 |
| PIE、多人、Cook及视觉验收 | 用户本轮禁止 | 后续原生运行、打包和人工审核 |
| Git提交与远程推送 | 未执行 | 根据明确交付要求和其他任务冲突处理后独立审查 |

今后继续必须先满足真实资产、引擎与发布门禁，尤其不得为达到“全部完成”而跳过原生验证、误删软引用资源或用游戏端客户端接口代替服务器权威结果。
