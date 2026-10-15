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
- 默认最大存活 4 只，遍历出生点周边 26/34/42 米的候选环，12 个方位，验证实际 NavMesh 地面、坡度、胶囊占位、距任意 PlayerStart 至少 22 米、怪物彼此至少 5 米。若 NavMesh/安全地面缺失则不生成。
- AI 自动巡逻、服务器视野与候选、追击、离出生地太远脱战返回，近战上限 185 厘米、12 点基础物理伤害、近战射线无遮挡；距任何玩家出生点 20 米以内不允许咬击玩家。上述数值是首轮测试基线，正式产品应移入项目怪物定义。
- 狼的最大生命 75；死亡胶囊关闭，4 秒销毁尸体，14 秒刷新，离开世界/关闭专服/体验代次变化撤销计时器和自有 Actor。多玩家只由专服结算战斗；客户端只显示复制结果。
- 与开发石灵的独立性：`ADivineBeastsDevelopmentMonster` 仍然严格 `CanSourceCombat()==false`，测试狼不修改或激活该被动训练目标。

## 资产实际导入顺序（需要已登录 Epic 账户）

1. 在 Fab 页面将 ANIMAL VARIETY PACK **Add to My Library（添加到资料库）**，保留订单/许可证凭证与真实产品 ID。
2. 在本机 UE5.8 Editor 打开 `E:\\poject\\feebooz\\DivineBeastsWorkspace\\Game` 工程，用 `Window > Fab` 登录同一 Epic 账号，从 My Library 将对应资产合法添加到项目。若仅支持引擎项目而非内容插件，先导入临时内容目录，再**使用 Editor 的资产移动工具和 Fix Up Redirectors**迁入唯一正式内容包（不通过资源文件系统直接重命名 .uasset）。
3. 检查狼的实际 SkeletalMesh、Skeleton、PhysicsAsset、材质/贴图与26段实际存在的动画。建立或迁移正式狼 AnimBlueprint，确保骨架匹配、Idle/Walk/Run/Attack/Hit/Death 动画能在 Editor 预览。若动画内容不足，应记录差异而不是声称完整。
4. 在 Editor 原生新建 `UGamePlatformAIDefinition` 主数据资产（推荐 `/DBAWorldPack_Village/AI/DA_DBA_VillageWolfAI`），`BrainType=None`，`AIDefinitionId=sslm.ai.village.wolf`，`SightRadius=1400cm`，`LoseSightRadius=1800cm`，`HomePolicy.LeashRadius=2200cm`，`PatrolRadius=500cm`，`AttackRange=185cm`，`PreferredRange=140cm`，`PrimaryAbilityTag` 留空走服务器原生近战，`DecisionInterval=0.35s`，`AttackRetryBackoff=1.5s`。
5. 在 `Game/Config/DefaultGame.ini` 的 `[DivineBeasts.VillageWolves]` 填入真实 `WolfAIDefinition`、`WolfSkeletalMesh`、`WolfAnimationBlueprint` **完整 UObject 路径**（AnimBP 以 `_C` 结尾），并仅在双端内容 / Cook 真实验收通过后将 `bClientVisualsVerified=true`、`bEnableVillageWolves=true`。
6. 构建 Village NavMesh 并验证实际 PlayerStart；专服联机双客户端从真正 Boot→Login→Village 进入，目测四只狼实际分散在安全区外围，狼感知/追击/攻击/死亡/刷新、回家与站位、网格/动画/材质正确；断线重连与多人同时攻击不能复制刷怪或双重结算。正式环境需对全部这些用例提供日志/截屏及最小Cook证据。
7. 正式启用前完成许可归档：`FabListingUrl`（资源链接）、`Owner`（卖方）、`LicenseType`（授权类型）、`AcquiredAt`（领取日期）、`ImportedAssetPaths`（实际资产路径）、`AssetHash`（来源校验）及商用许可核对。

## 当前阻塞 / 交付判定

- 已建立多层 C++ 集成与默认关闭门禁，但**未取得外部 Fab 源资源、没有真实狼蓝图、动画状态机、AI 定义 .uasset、内容包注册及可见关卡验收**；默认禁用且未宣称战斗已经在地图运行。
- 真实部署需要用户在此 Editor 的 Fab 窗口完成 Epic 授权登录/领取，否则第三方产品的二进制资产不可能由无权限的源代码检索替代。
- 本文件属于实施记录，不代表实际编译完成；编译、自动化与双机专服验证结果应以本轮实际运行日志为准。
