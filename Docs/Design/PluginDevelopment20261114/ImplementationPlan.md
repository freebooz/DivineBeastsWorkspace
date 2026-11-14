# SSLM 插件开发关卡、资源包与命名统一执行计划

> 执行者逐项采用 superpowers:executing-plans；每项单独提供证据。当前仅完成审查和设计交付，工程资产实施等待用户对设计的人工确认。不得把未勾选步骤视作已完成。

**Goal：** 让全部 70 个现有插件从同一个 HUB 进入各自工作台和独立调试场景，完成操作、退出及留证，同时建立 8 类资源审核场景和可追溯的无下划线文件命名规则。统一入口以 PluginHubDesign.md 为准。

**Architecture：** 复用现有 DivineBeastsDemoClient、适配器、目录和 70 个 Widget 页面；新增薄根关卡，按领域共享测试设施。GamePlatform 提供中立机制，MobaCommon 仅扩展通用竞技，DBA 负责项目装配和内容，依赖单向且同层无环。

**Tech Stack：** 当前锁定 UE 5.8、Monolith MCP、C++、UMG、AssetRegistry、原生内容插件、现有 PowerShell 构建与 Cook 入口。

**Spec：** [设计规格](DesignSpec.md)、[命名规范](NamingStandard.md)、[审查报告](AuditReport.md)、[逐关卡设计](Review.html)。完整路径与现有能力清单见 LevelCatalog.json、PluginMatrix.csv、MeshInventory.csv。

## 全局约束

- 新建人工维护文件用 PascalCase；资源为类型前缀＋归属＋用途＋必要变体，无下划线。工具链、第三方来源和已发布身份按 NamingStandard 登记例外。
- 仅使用本地工具和直接 Monolith；不使用 HWebCodex，不复制另一套插件执行器或运行注册表。
- 真实资产只能由 UE 创建和迁移；改名、引用保存、冷重载及发布路径验证属于同一批次。
- UI 初次读取快照，随后事件通知；换世界、失活、取消与来源换代必须解绑和清理。
- 现有正式世界的地形、湖泊、导航、天气和手工摆放不在本方案改动范围；所有输出留在当前工作空间。
- 参数取现有 Definition/接口真源，禁止把设计图的示例数值当作默认值或实测结果。
- 只增量构建受影响目标。Editor 使用既定 NoSharedPCH、NoUBA、MaxParallelActions 8、UsePrecompiled 和 AdditionalPlugins DBAServer；Client/Server 不使用 UsePrecompiled。
- 人工验证实际回读 t.MaxFPS=0、r.VSync=0。没有 GPU 测量不宣称性能达标。

## 重点回归条件

1. 同名候选、大小写折叠与重定向链：重名必须在写入前拒绝，旧引用不能静默丢失（任务 1、6）。
2. 页面显示成功但后台依赖不可用：显示真实阻塞，不产生成功记录（任务 3、4）。
3. 准备中取消、重复执行、换世界后迟到回调：不恢复旧 UI，不泄漏租约、不重复结果（任务 3、4）。
4. 新页面没有主题、隐藏后重显或输入设备切换：固定字号、初始样式、焦点与事件重新绑定正确（任务 2）。
5. 内容包的装饰与权威碰撞共存：正式客户端不带开发地图，服务端保留必要碰撞且剥离纯 UI/VFX（任务 5、6）。

## 任务 1：冻结迁移清单和引用范围

**文件：** 读取当前 AGENTS.md、ContentPackRegistry.json、Tools/Unreal/Resources/ResourceOwnershipManifest.json；更新 Docs/Implementation/PluginDemos/PluginDemoManifest.json 和本轮 RenameCandidates.csv 的批准状态。总体目录说明同步记录变化，不改反射/协议稳定身份。

**接口：** 输入为批准的旧包路径→新包路径及归属；输出为每批唯一清单、源文件哈希、引用者和回退路径。LevelCatalog.json 只作为审批索引，批准字段合入既有 manifest，不建立第二个活动配置。

- [ ] 复查当前分支、未提交修改、编辑器 PID/工程/地图/脏状态与其他任务占用，仅获取本次资产写入范围。
- [ ] 对候选按真实类型、用途和所有者确定最终名称；先核对两个 kachujin 材质的差异及引用，不能简单删下划线后覆盖。
- [ ] 枚举硬引用、软路径、配置、作者脚本、验证脚本、地图外置对象和 Cook 清单。记录所有存量兼容例外及保留原因。
- [ ] 在工作区 Saved 下保存本批原件与哈希；重名、外部修改或未知所有权任一出现即停止该批，其他独立批次可继续。
- [ ] 运行大小写不敏感路径唯一性检查；确认旧路径拥有明确迁移/保留结果，不能以候选 CSV 作为直接执行输入。

**本项通过条件：** 批次中每个新名称语义明确、无冲突、来源可追溯、引用范围和回退文件齐备。

## 任务 2：PCG、Surface 和植物包三类原生试点

**文件：** 更新 Tools/Unreal/PluginDemos/PluginDemoUIMonolithRecipes.json、AuthorIndependentPluginPagesMonolith.py、AuthorPluginManualFixturesMonolith.py；复用 Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Public/UI/DivineBeastsDemoHubScreen.h 及其已有 Private/UI 实现。具体关卡/Widget 目标取 LevelCatalog.json 中 GamePlatformPCG、GamePlatformSurface、DBAFoliagePack_Core 三行。

**接口：** 页面沿用 FixedPluginId 和已有目录/运行 ViewModel；读取当前定义中的参数类型、单位、范围和操作资格。材料、植物模型及 PCG 图来自各自原所有者。

- [ ] 将三张批准布局映射到现有命名控件与事件；不改变业务接口和固定字号，不添加业务 Tick。
- [ ] 按 PluginHubDesign.md 原位改版现有统一 HUB；以三列大卡片、上图下说明及纵向滚动覆盖全部登记插件（不分页），不显示目录、搜索和侧栏详情。点击卡片进入页面且不自动执行案例；返回时恢复可见位置和卡片焦点。
- [ ] 通过 Monolith 原位改版现有 Widget；需要改名时使用引擎原生迁移并更新当前 manifest 和作者/验证器路径。
- [ ] 建立三张薄根关卡，只装配当前领域夹具；用相同测试种子验证 PCG 生成、坡度及道路/建筑排除，不改正式新手村。
- [ ] 验证首次快照、无主题初始样式、四态按钮、焦点、隐藏重显、取消和 EndPlay 清理；不支持的操作显示具名原因。
- [ ] 保存/编译后，用独立重载验证 Widget 父类、控件、FixedPluginId、软引用和实际地图入口；采集固定镜头截图及操作视频。

**本项通过条件：** 三类试点原生运行、保存重载和人工界面审核通过；其余页面复用同一成熟机制，不提前批量复制缺陷。

## 任务 3：补齐案例，再展开 70 个独立入口

**文件：** Docs/Implementation/PluginDemos/PluginDemoManifest.json、AuthorPluginDemoCatalog.py、AuthorWorldManualFixturesMonolith.py、AuthorGameplayManualFixturesMonolith.py、AuthorPresentationFixturesMonolith.py；必要适配修改位于现有 DivineBeastsDemoClient/Private/Demo 对应领域文件。

**接口：** 保留 IDivineBeastsDemoScenarioAdapter 的 Prepare(const FDivineBeastsDemoScenarioContext&, FDivineBeastsDemoOperationCompletion)、Execute(FName, const TArray<FDivineBeastsDemoParameterValue>&, FDivineBeastsDemoOperationCompletion)、Stop(FDivineBeastsDemoOperationCompletion)。工厂仍按 UWorld 创建独立适配实例。

- [ ] 为当前缺登记案例的 9 个插件补真实最小操作合同：DBAArena、DBAGameplay、DBAServer、DBAWorlds、GamePlatformLocalization、GamePlatformLobby、GamePlatformVillage、GamePlatformArena、MobaPresentation；不得仅加空地图。
- [ ] 逐项核对 211 个既有案例的工厂、操作、参数、资格及结果，更新失真的 registered/wired 等状态，保留旧证据时间与输入版本。
- [ ] 按已批准目录将 70 个现有页面对应到独立根入口；共享 Hub 继续导航，共享模型/材质/夹具不复制。已有可用独用关卡优先原生迁移/引用复用。
- [ ] 每批运行准备中取消、重复执行、失败恢复、换世界迟到回调和多次进入退出；检查本次句柄和租约终止，其他世界不受影响。
- [ ] 每批生成目录资产并编译保存，再使用 ValidateIndependentPluginPagesMonolith.validate_pages(require_fresh_editor=True) 与 ValidatePluginDemoCatalogAsset.main(require_fresh_editor=True) 独立冷重载。

**本项通过条件：** 每插件拥有准确入口、实际可用或具名受阻的操作；70 页身份互不串用；代码注册、manifest、真实目录资产一致。AuditPluginUIReadiness.py --summary 仅辅助核对，不代替运行证据。

## 任务 4：区分编辑器、服务端及客户端资格

**文件：** 现有 PluginDemoManifest 与 DivineBeastsDemoClient 中编辑器/服务/世界适配器；Tests/Architecture/InheritanceBoundaryAudit.psm1 作为边界检查，禁止为页面方便公开 Private 类型。

**接口：** 继续调用领域公开服务与事件；第三方蓝图布局工具仅在编辑器宿主执行；服务器和经济动作保持现有权威、权限、幂等和取消合同。

- [ ] 为 BlueprintAutoLayout、GlooPrint 显示选中图、只读预检、差异及 Undo；实际编辑必须使用其既有编辑器实现，不宣称打包客户端可执行。
- [ ] 对 Online/Session/Server/竞技/交易类注入真实测试依赖；不可用时阻止命令并显示具体缺项，凭据不展示或落盘。
- [ ] 验证依赖丢失、版本不匹配、拒绝资格、取消与重入，不产生伪成功、重复交易或重复服务器动作。
- [ ] 运行既有继承/依赖边界门禁，并通过原生引用审计补证蓝图和 DataAsset 引用方向。

**本项通过条件：** UI 所显示资格等于服务真实资格，模块/资产不反向依赖，失败与成功证据分开。

## 任务 5：资源目录、两个公共包和八个资源场景

**文件：** 复用 DBAFoliagePack_Core；首批真实内容齐备后新增 DBAEnvironmentPackCore 与 DBAArchitecturePackCore 的叶子描述符及 Content；更新 ContentPackRegistry.json、ResourceOwnershipManifest.json 和已有 ResourceOwnership 交付记录。八张资源场景目标见 LevelCatalog.json 的 ResourcePlan 行。

**接口：** 内容仅提供定义和软引用，GamePlatformData 持有加载/租约权威；公共材质函数沿用 GamePlatformSurface，项目视觉留在第三层。

- [ ] 对 MeshInventory.csv 中现有模型按来源、授权、实际用途、厘米单位、轴心、Bounds、槽位、LOD/Nanite、碰撞、纹理通道与风合同补齐真实检查结果。
- [ ] 以引用分类已有 41 个植物网格，12 个供应商花模型不重复导入；灌木和正式岩石缺口按 ResourcePlan.csv 补充已获许可的真实来源。
- [ ] 首批自然环境与建筑构件达到来源/依赖/目标隔离要求后注册两个内容包，不提前生成空包。世界专属草屋、道路和摆放保持原位，确有公共复用需要才纳入迁移批次。
- [ ] 通过 Monolith 建立八个资源场景，分别核对树/灌木草/花/石/建筑/构件/道具/材质；构件检查真实开口和吸附，材质使用固定曝光及正确 sRGB/通道。
- [ ] 原生保存、独立重载，验证切换只释放本次租约；测量代表场景的 CPU/GPU、实例、Draw Call、透明过绘和加载峰值，记录设备和设置，不预设收益。

**本项通过条件：** 资源拥有唯一所有者和真实来源，分类不复制模型，八类场景可独立检查，必要权威碰撞不因表现拆包丢失。

## 任务 6：统一改名的引用、构建和发布门禁

**文件：** 本批唯一映射、作者/验证器脚本、现有构建和 Cook 入口、服务器表现排除清单及内容注册。不得仅修改资产文件扩展名或使用文件系统搬运 uasset/umap。

- [ ] 使用引擎 RenameAssets 执行已审查映射，保存真实引用者；更新文本软路径和单一作者入口，不留下旧脚本能再次发布旧名字。
- [ ] 独立冷重载后检查旧引用残留和 Redirector；只有所有引用收敛后才通过引擎处理重定向，不手删。
- [ ] 只在 C++ 受影响或缺少当前完整目标证据时，经 Build/Game/BuildFoundation.ps1 按固定参数增量构建；静态文档改名不触发重编译。
- [ ] 对开发宿主和受影响发布目标运行既有 Cook/Stage 门禁，输出均在工程内；检查正式客户端不带开发关卡/UI，专服不带纯表现且权威定义/碰撞仍可加载。
- [ ] 分别记录源码、资产编译、保存重载、完整目标、Cook 容器及运行结果，任一失败不得以脚本退出 0 代替。

**本项通过条件：** 新路径可在冷加载与实际目标中解析，旧活动写入入口已收敛，无破坏性身份变化，无第二套活动实现。

## 任务 7：最终人工审核

- [ ] 输出逐插件真实状态矩阵、各关卡固定镜头原生截图、关键操作视频、性能实测与全部未完成项。设计图和实机图分目录并明确标识。
- [ ] 需要联机的案例先核对 Client 与真实 Village/MainArena 构建及 Cook/Stage 一致，再沿用 StartLocalTestClients.ps1 从登录页进入，核对十个可用快速登录按钮；不将窗口启动当准入。
- [ ] 提交用户逐批人工确认；只把已确认且所有必要门禁通过的项标为验收，不自动替换正式世界，不提交主分支。

**当前停止点：** 本文、命名规范及 78 张布局图已经可审阅。以上工程实施步骤仍未执行。
