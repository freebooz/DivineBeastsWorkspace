# 2026-10-16 状态补充

下文保留2026-10-14的Fab方案历史。当前已改为接入作者Quaternius发布的CC0狼，未领取或导入PROTOFACTOR产品。实际路径、两段动画、导航与Cook隔离见[当前修复与操作说明](VillageWolvesAndPartyOperations20261016.md)，不要再以旧阻塞说明推断当前资产为空。

# SSLM 新手村 ANIMAL VARIETY PACK 狼怪物接入记录（2026-10-14）

## 目标、授权与当前证据

- Fab（虚幻引擎资源平台）官方资源：PROTOFACTOR INC. 的 [ANIMAL VARIETY PACK](https://www.fab.com/listings/2dd7964c-a601-4264-a53d-465dcae1644c)。狼为四足生物，约 33 根骨骼、26 段原始动画，最终以下载包实际内容为准。
- 源素材需要当前 Epic/Fab 账户的正式领取资格和本机 Fab 插件下载流程。当前 Windows Runner 的 Epic Games Launcher **未运行**；项目中**未检测到**来自本包的狼 *.uasset 或原始网格文件。因此目前**并未完成 Fab 素材的真实下载与导入**，也未取得实际狼骨骼/动画的 Editor 验收证据。
- 商业游戏中可以依 Fab 购买时的许可使用，但不能把 Fab 源素材作为可单独销售/分发的模型包公开镜像；如需提交私有版本库须确认团队授权、许可证版本和素材再分发条件。
- 未提供原始二进制素材以前禁止伪造 `.uasset`、复制别人的狼占位或虚报 26 段动画已经能运行。

## 三层所有权与插件化

- **平台基础层 GamePlatformAI/GamePlatformCombat/GamePlatformData/Navigation**：原有通用感知、仇恨候选、状态机、巡逻/返回、Combat/GAS、服务器命中及动态资源租约；本轮增加原生轻量 BrainMode(None) 扩展点，BT/StateTree 的既有逻辑不变。
- **MOBA 通用层**：新手村狼并非竞技角色，不引入 MOBA 对局、队伍或竞技规则；未来跨 PvE/MOBA 怪物可通过平台中立定义复用，不能把狼写死到竞技插件。
- **项目层 DivineBeastsCharactersRuntime（双端）**：`ADivineBeastsVillageWolf`，狼怪物实体、GAS/Combat生命门禁和复制死亡；不持有第三方美术。
- **正式玩家目标资格**：`ADivineBeastsCharacter` 统一装配 `UGamePlatformAITargetComponent`（平台 AI 目标标识），在服务器身份 Ready、玩家实际控制、死亡与重生时更新资格；狼等不由玩家控制的实体不会作为玩家目标暴露。
- **项目层 DBAServer（ServerOnly）**：`ADivineBeastsVillageWolfAIController`（服务器咬击和安全区复核）和 `UDivineBeastsVillageWolfWorldSubsystem`（实际 Village.Tutorial 专服门禁、散布、死亡补充和清理），不引用客户端纹理和动画。
- **项目层 DivineBeastsPresentationClient（ClientOnly）**：`UDivineBeastsVillageWolfVisualWorldSubsystem`。经 GamePlatformData 世界租约异步加载 SkeletalMesh 与 AnimBP 的有效真实资源，仅客户端驱动动画。
- **纯内容包**：取得正版源素材后，以真正第三方资产创建单一独立插件 `DBAContentPack_AnimalsProtofactor`（通用动物美术包，狼／鹿／猪及共用骨架与材质；不携带服务器AI代码），挂到 `Game/Plugins/DivineBeasts/ContentPacks/Common/` 下再登记进 `ContentPackRegistry.json`。未经真实资产交付不得创建空的插件或登记。项目行为/区域、AI 定义和具体新手任务归 `DBAWorldPack_Village`（新手村世界内容包）。
- **Cook 边界**：动物 Mesh、材质、贴图、动画只由 Client/Editor 内容组合选用；Dedicated Server 保留 Village.umap、出生点、NavMesh、AI轻量定义、GAS及碰撞。严禁服务器 Cook 引入整个动物美术包。

## 当前实现的可配置玩法基线

- 场景：只允许平台 World Ready、ServerLifecycle Ready 且 ActiveExperience = Village.Tutorial 的 Dedicated Server；不会进入 OpenWorld、MainArena 或客户端独立游戏进程。需要 Village.Main 时应先确认共享 Catalog 中存在正式该体验并单独加门禁。
- 最大同时存活20只；候选位置以PlayerStart为中心，从45米至290米扩展8圈、每圈24方向，检查真实NavMesh、地面坡度及胶囊占位，距任何PlayerStart至少22米；任意两只狼的原始出生锚点水平距离严格大于30米。真实地图空间不足时保留少量狼，不强刷满20只。
- AI 自动巡逻、服务器视野与候选、追击、离出生地太远脱战返回，近战上限 185 厘米、12 点基础物理伤害、近战射线无遮挡；距任何玩家出生点 20 米以内不允许咬击玩家。上述数值是首轮测试基线，正式产品应移入项目怪物定义。
- 狼的最大生命 75；死亡胶囊关闭，4 秒销毁尸体，14 秒刷新，离开世界/关闭专服/体验代次变化撤销计时器和自有 Actor。多玩家只由专服结算战斗；客户端只显示复制结果。
- 已退役的远古石灵与本狼群实现没有互相依赖；狼只复用现存通用Combat/GAS及平台AI，不重新引入石灵。

## 2026-10-15 狼群散布扩容变更

用户最新要求：**新手村狼群最多同时存活20只；任意两只狼的原始生成位置水平距离严格大于30米**。这是生成点间隔，不强制约束战斗中追击相互靠近的狼。

- 参数在 `DBAServer/Private/Village/DivineBeastsVillageWolfSpawnPolicy.h`（专服私有散布策略）：`MaxAliveWolves=20`（存活上限）、`MinimumSpawnSeparationCm=3000`（距离必须超过3000厘米）。
- 旧26/34/42米三圈空间不足，现改为45米起、每圈外扩35米、8圈（最远290米）、每圈24方向的有限候选，多出生点轮换顺序。
- 每个候选经真实 NavMesh（导航网格）、全部 PlayerStart（玩家出生点）的22米禁刷区、地面坡度、实体胶囊碰撞检查，最终落点按水平XY再核间距；缺乏安全陆地或导航时允许少于20只，绝不强刷。
- 专服记录每只狼的出生锚点，移动不释放间隔名额；死亡尸体不计存活数量但仍保留锚点，4秒退场后撤销；按原有14秒延迟补怪时重新校验距离。
- `TryFillWolves`（狼群补充）按实际存活数量计数；第二轮改为每批最多4只、每隔0.2秒补充，不新增逐帧扫描；死亡/世界退出取消未执行的批次。
- 原生回归 `DivineBeasts.Server.VillageWolf.SpawnPolicyTwentyAndThirtyMeters` 测试恰好30米拒绝、30.01米可接受、XY距离与安全区、理想平地可布置20只；不等于真实NavMesh、Shader、Fab资源或多人验收。
- 来自 ANIMAL VARIETY PACK 的真实狼骨骼、模型、贴图和动画仍未导入；项目层AI定义和角色蓝图已在后续UE5.8作者步骤中原生保存。`bEnableVillageWolves=false`、`bClientVisualsVerified=false` 保留，不因扩容而生成无外观攻击怪。

### 2026-10-15 实际验证证据

- `python Tests/Architecture/ValidateVillageWolfScatter20.py`（静态/理想平地测试）成功：上限20、严格>30米、20个理想地面候选、实际候选最短35米、8圈×24方向，0问题；该测试不加载真实NavMesh。
- `git diff --check`（修改差异空白检查）、Python脚本语法校验通过。
- 定向UE5.8 Editor DBAServer模块构建成功，退出码0，证据：`Saved/Validation/FoundationM0/843e31bd-ef2f-4681-ae75-25afa6078380/Build`。该步骤不是完整Editor目标构建。
- 完整Editor首先因引擎已安装的第三方Monolith源码错误而失败；之后使用UE5.8官方UBT构建参数`-DisablePlugin=Monolith`（本次构建禁用第三方插件）及`-EnablePlugin=DBAServer`（启用服务器模块）成功生成完整Editor目标。没有修改第三方源码。
- UE5.8原生测试 `DivineBeasts.Server.VillageWolf.SpawnPolicyTwentyAndThirtyMeters`：引擎日志报告Found 1，Executed 1，结果Success，退出码0；独立读取 `Saved/Validation/VillageWolfScatter20261015/Native-485a16ee-50e7-456e-b371-e339eeac37d2/Report/index.json`（原生测试结果）确认恰好1项且状态Success。
- `DivineBeastsArenaServer`（实际专用服务器目标）构建与Cook（资源烘焙）、新手村地图真实20狼位置、客户端美术与联机运行仍需分别确认，不以理想平地通过替代实景验收。

### 2026-10-15 第二轮：代码与项目原生蓝图制作（按要求不编译验证）

- 已补齐 `UDivineBeastsVillageWolfWorldSubsystem`（服务器世界刷狼子系统）的四只一批、0.2秒间隔生成，保持总量不超过20只；只在服务器Ready及分批Timer执行。死亡后取消剩余批次，继续保留14秒重生延迟与4秒尸体清除。出生锚点的水平距离必须严格超过30米。
- `WolfBlueprintClass`（服务端狼怪物蓝图软类）为新配置项，服务端只加载无美术依赖的项目蓝图，不在Dedicated Server加载狼网格/贴图/动画。服务端许可同时要求该原生蓝图与AI定义真正存在。
- 已使用UE5.8编辑器的 `-ExecutePythonScript` 作者模式，原生保存 `/DBAWorldPack_Village/AI/Blueprints/BP_DBA_VillageWolf`（服务器安全狼角色蓝图）和 `/DBAWorldPack_Village/AI/Definitions/DA_DBA_VillageWolfAI`（狼AI数据资产）。编辑器输出 `SSLM_WOLF_CORE_NATIVE_SAVED` 两次及作者完成标记；对应原生文件为 `26600` 字节和 `2887` 字节（首次作者记录）。
- 资源作者脚本 `Tools/Unreal/Creatures/AuthorVillageWolfCoreUE58.py`（UE5.8狼原生作者）及 `VillageWolfAuthoringSpec.json`（生产参数合同）已纳入工作空间；作者证据为 `Saved/Validation/VillageWolf/Authoring/CoreAuthorManifest.json`。没有通过复制文本扩展名伪造`.uasset`。
- `Game/Config/DefaultGame.ini`（游戏配置）中的 `WolfAIDefinition` 和 `WolfBlueprintClass` 已配置到实际创建的软资源对象路径；但 `bEnableVillageWolves=false`、`bClientVisualsVerified=false` 保持关闭，避免出现不可见却能攻击玩家的怪物。
- 真正来自 Fab 的狼SkeletalMesh（骨骼网格）、Skeleton（骨架）、PhysicsAsset（物理资产）、材质贴图和与狼骨骼一致的 `WolfAnimationBlueprint`（狼动画蓝图）**仍未导入**。动画状态机必须绑定真实骨架，不能在无资产时用Manny骨架或伪uasset替代。`InspectVillageWolfVisualSourcesUE58.py`（美术来源检查作者）为后续导入检查入口。
- 本轮用户明确要求不进行编译验证，因此本轮新增C++、新保存的AI定义/角色蓝图未执行新一轮UBT编译、Shader编译、自动化测试、Cook或多人运行验收。上一轮已完成的几何/定向编译结果仅作为历史记录，不代表本轮新变更已经验证。

## 资产实际导入顺序（需要已登录 Epic 账户）

1. 在 Fab 页面将 ANIMAL VARIETY PACK **Add to My Library（添加到资料库）**，保留订单/许可证凭证与真实产品 ID。
2. 在本机 UE5.8 Editor 打开 `E:\\poject\\feebooz\\DivineBeastsWorkspace\\Game` 工程，用 `Window > Fab` 登录同一 Epic 账号，从 My Library 将对应资产合法添加到项目。若仅支持引擎项目而非内容插件，先导入临时内容目录，再**使用 Editor 的资产移动工具和 Fix Up Redirectors**迁入唯一正式内容包（不通过资源文件系统直接重命名 .uasset）。
3. 检查狼的实际 SkeletalMesh、Skeleton、PhysicsAsset、材质/贴图与26段实际存在的动画。建立或迁移正式狼 AnimBlueprint，确保骨架匹配、Idle/Walk/Run/Attack/Hit/Death 动画能在 Editor 预览。若动画内容不足，应记录差异而不是声称完整。
4. 本项已由UE5.8完成原生作者创建：`/DBAWorldPack_Village/AI/Definitions/DA_DBA_VillageWolfAI`（原生AI数据资产），`BrainType=None`，`AIDefinitionId=sslm.ai.village.wolf`，`SightRadius=1400cm`，`LoseSightRadius=1800cm`，`HomePolicy.LeashRadius=2200cm`，`PatrolRadius=500cm`，`AttackRange=185cm`，`PreferredRange=140cm`，`PrimaryAbilityTag` 留空走服务器原生近战，`DecisionInterval=0.35s`，`AttackRetryBackoff=1.5s`。同批创建服务器安全狼角色蓝图 `BP_DBA_VillageWolf`，尚未正式编译或游戏内验证。
5. `Game/Config/DefaultGame.ini` 中 `WolfAIDefinition` 与 `WolfBlueprintClass` 已填入真实对象路径；待Fab导入后还需补齐 `WolfSkeletalMesh`、`WolfAnimationBlueprint` 的真实 UObject 路径（AnimBP以 `_C` 结尾）。只有完成真实资源、动画、Client/Server Cook和双端联机验收，才能将 `bClientVisualsVerified=true`、`bEnableVillageWolves=true`。
6. 构建 Village NavMesh 并验证实际 PlayerStart；专服联机双客户端从真正 Boot→Login→Village 进入，在可用地形和导航充足的条件下验收最多20只狼的实际散布及>30米出生锚点间距，并核实感知、追击、攻击、死亡、刷新、回家、网格/动画/材质；空间不足时以服务器拒绝原因与实际数量记录，不伪造20只。断线重连与多人攻击不得双重刷怪或结算。必须保留日志/截屏及最小Cook证据。
7. 正式启用前完成许可归档：`FabListingUrl`（资源链接）、`Owner`（卖方）、`LicenseType`（授权类型）、`AcquiredAt`（领取日期）、`ImportedAssetPaths`（实际资产路径）、`AssetHash`（来源校验）及商用许可核对。

## 当前阻塞 / 交付判定

- 已建立多层C++及默认关闭门禁，并使用UE5.8创建了**真实服务器安全狼角色蓝图和AI定义 .uasset**；但仍未取得外部 Fab 狼模型、骨骼和动画状态机，没有通用动物美术包登记，也未进行本轮编译/Cook与真实新手村可见联机验收；因此继续默认禁用。
- 真实部署需要用户在此 Editor 的 Fab 窗口完成 Epic 授权登录/领取，否则第三方产品的二进制资产不可能由无权限的源代码检索替代。
- 本文件属于实施记录，不代表实际编译完成；编译、自动化与双机专服验证结果应以本轮实际运行日志为准。
## 2026-10-16 续作：动物资源导入与动画绑定流程

本轮按最新指令不执行编译、Cook及自动化测试。所有未执行的步骤必须如实保留状态，不用历史测试通过替代新增代码的验证。

- Fab 官方免费资源为 <https://www.fab.com/listings/2dd7964c-a601-4264-a53d-465dcae1644c>（ANIMAL VARIETY PACK）。Epic Games Launcher（启动器）正在运行，另一套 UE5.8 安装有 Fab UE Plugin（Fab编辑器插件），但当前实际项目、源引擎和已安装清单未发现此动物包的狼骨骼/动画；没有可合法直接迁移的素材。
- 新增 `Tools/Unreal/Creatures/LicensedAnimalVarietyImport.json`（已授权Fab来源清单）。由用户的Fab账户领取资源后填写许可记录路径、原生导入目录、Wolf SkeletalMesh（狼骨骼网格）、Skeleton（骨架）、PhysicsAsset（物理资产）、AnimBlueprint（动画蓝图）及关键动画路径；不要在仓库公开令牌或私有凭证。
- 新增 `Tools/Unreal/Creatures/StageLicensedAnimalVarietyUE58.py`（通用动物内容包迁移作者），设计为inspect（只读）、prepare（合法真实资源预检及插件描述准备）、migrate（重启编辑器后通过UE原生资产重命名将完整动物包迁入独立插件）。只有迁入有内容后才登记项目内容包注册表和游戏客户端插件，专服不携带狼材质与模型。
- 新增 `Tools/Unreal/Creatures/BindLicensedVillageWolfVisualUE58.py`（狼客户端表现软资源绑定作者）。真正迁移后再读取并检查Mesh与AnimBlueprint是否使用相同骨架，成功才填入游戏配置路径；两个安全开关继续为false，等待人工审核和正式Cook。
- 新增 `Tools/Unreal/Creatures/InspectVillageWolfMapUE58.py`（只读关卡清单作者）。编辑器读取尝试在引擎启动阶段出现Fatal error: Null assigned to TNotNull，没有生成地图PlayerStart/NavMesh清单，也未修改关卡。不得据此认定导航存在或地图损坏。
- 已有两份 UE5.8 原生资产仍保留：服务器安全狼角色蓝图 BP_DBA_VillageWolf，狼AI定义 DA_DBA_VillageWolfAI。服务端限制最多20只，原始生成点水平间距严格超过30米，22米出生安全区与14秒重生冷却保持不变。

**交付边界：** 当前新增的授权迁移与软引用绑定代码已写入，但由于没有实际Fab正版原始素材，通用动物美术包及可驱动的狼动画蓝图尚未实际创建；本轮没有编译，游戏内仍不会出现狼。需要用户在Fab My Library中完成实际领取/导入，后续才能完成最后的资产迁移及显示。
## 2026-10-16 第二轮：完成散布合并冲突和动画源兼容性处理

本轮仍遵守用户明确要求：只写代码和作者资产配方，不编译、不Cook、不运行游戏或自动化测试。

- 重新读取最新主分支，发现 DBAServer（项目专用服务器）狼群子系统源文件夹杂旧版5米间距逻辑的Git合并冲突标记。已仅在该狼文件中移除全部实际发现的标记，保留20只上限、30米严格间距与原始出生锚点，未覆盖其它正在进行的VFX任务。工作区只读审查索引冲突数为0，不代表C++编译已通过。
- 服务端 HasVerifiedAnimalAssets（动物资源许可门禁）额外核对 WolfSkeletalMesh（狼网格）及 WolfAnimationBlueprint（动画蓝图）的非空软路径，并限定两个引用属于 DBAContentPack_AnimalsProtofactor（通用动物美术内容包）。服务端只检查字符串结构，不装载客户端第三方骨骼、材质或贴图。这样即使有人提前将两个开关改为true，资源路径为空也会拒绝生成透明攻击敌人。
- DivineBeastsVillageWolfVisualWorldSubsystem（客户端狼表现世界子系统）增加与真实UAnimBlueprintGeneratedClass（动画蓝图生成类）的Skeleton（骨架）一致性检测；只接受与狼SkeletalMesh骨架完全相同的动画蓝图，不接受其它动物的错误绑定。
- 动物内容包迁移作者 StageLicensedAnimalVarietyUE58.py 现在允许 Fab 包只有原生Wolf Skeleton、SkeletalMesh及六类关键动画序列，PhysicsAsset（物理资产）与 AnimBlueprint（动画蓝图）可在授权资产导入后另行制作；不能因此阻断真实狼资源的迁移。
- 增加 AuthorLicensedWolfAnimBlueprintUE58.py（狼骨架专属动画蓝图底稿作者），使用UE5.8 AnimBlueprintFactory（原生动画蓝图资产工厂）与真实Wolf Skeleton/PreviewMesh建立动画蓝图底稿。底稿尚无完整AnimGraph（动画状态机），状态仍为 SkeletonOnly_NoAnimGraph，绝不冒充完整可运行动画，也不启用刷怪。
- 统一修正 InspectVillageWolfVisualSourcesUE58.py（美术资源只读检查）读取单一正版Fab清单，避免与另一份旧清单中的路径发生冲突；客户端绑定工具 BindLicensedVillageWolfVisualUE58.py 已允许从源Fab自带AnimBlueprint，或从后续创建的项目动画蓝图两条路径读取。两条路径都不自动批准动画图验收。
- 新建 WolfAnimationGraphRecipeV1.json（狼动画图制作规范），明确Idle、Walk、Run、Attack、Hit、Death状态、速度/死亡/攻击转换、音效通知与四足骨骼要求，用于后续Editor/Monolith逐项制作，当前只是原生图作者输入规范。
- 调整迁移脚本的 VillageDevelopmentClient（新手村开发客户端）AlwaysCook（强制烘焙）条目：必须写入明确的ProjectPackagingSettings（项目打包设置）配置段，不能盲目追加到文件尾部其它配置段。
- 更新下一阶段静态验收源码，加入狼视觉资源软路径及授权包归属检查；本轮未执行该测试。

**最终阻塞**：当前会话仍未获得用户Epic/Fab账户合法领取的ANIMAL VARIETY PACK实际狼网格、骨骼、动画二进制资产；因此真实动物内容插件、完整动画状态机、新手村地图生成点和游戏实测仍无法完成。当前bEnableVillageWolves=false、bClientVisualsVerified=false安全关闭。后续只能在用户完成Fab领取和UE正式导入之后继续真实资产制作、再由用户另行授权编译验证。
## 2026-10-16 第三轮：资源生命周期、客户端包装与调试提示

按最新指令继续只实施代码与作者流程，仍不执行UBT（虚幻构建工具）、Shader（着色器）编译、Cook（资源烘焙）、自动化测试和实机运行。以下修改均属于已写入源码、**待后续编译验收**。

- `DivineBeastsVillageWolfVisualWorldSubsystem.h/.cpp`（客户端狼表现世界子系统）增加每世界最多一次Fab狼资产租约申请限制，避免最多20只狼导致重复加载；资源配置错误只输出一次错误说明。与骨架不匹配的已加载模型和动画租约立即释放，防止内存持续占用；世界退出时仍统一清理租约和观察事件。
- 狼客户端骨骼模型启用SkeletalMesh动画更新率优化（Update Rate Optimization，骨骼动画更新频率优化），不可见时停止刷新完整姿态。这不改变Dedicated Server的服务器权威移动、伤害及碰撞。
- 客户端提供可选的 `WolfVisualYawDegrees`（狼模型Yaw朝向）、`WolfVisualScale`（狼模型缩放）、`WolfVisualHeightOffsetCm`（狼模型高度偏移）三项美术校准键；`Game/Config/DefaultGame.ini`仅留注释示例，默认不覆盖原组件朝向及大小。仅真实Fab网格人工审核后可开启，数值必须有限且受到上下限约束。
- 服务端新手村World开始时，如果狼资源/配置未满足生成许可，会仅记录一次 `VillageWolfInactive`（狼群未激活）和两项安全开关状态，帮助解释为什么地图看不到狼。没有因此启用透明攻击怪。
- `StageLicensedAnimalVarietyUE58.py`（授权动物资源原生迁移）在迁移时检查三层内容注册表、项目装配及两份客户端发布配置在迁移过程中是否被其他任务改动，发现变化拒绝覆盖；迁移成功后分别为 `VillageDevelopmentClient`（新手村开发客户端）和 `FrontEndClient`（正式前端客户端）加入 `Animals`（原包狼骨骼/纹理/动画）与 `Wolf/Animation`（后期动画蓝图）两个资源目录的客户端Cook配置。Dedicated Server纯表现隔离不变。
- 狼持续最多20只、生成点水平距离严格大于30米，PlayerStart（玩家出生点）周围22米保护区不变。未取得Fab真实资产之前 `bEnableVillageWolves=false`、`bClientVisualsVerified=false` 仍不改变。

**阻塞依旧：** 本轮工作区尚无用户Epic/Fab账号正式领取/下载的ANIMAL VARIETY PACK源狼网格、Skeleton及动画序列。实际动画图和通用动物插件不可凭JSON与蓝图底稿冒充。必须在用户授权后导入，才能继续真实资产创作。



