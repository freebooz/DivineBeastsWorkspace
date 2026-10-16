# GamePlatformDeveloperTools（游戏平台开发者工具插件）

定位：GamePlatform/Diagnostics（游戏平台层／诊断分类）的 Editor/CI 专用插件。

2026-09-27目录迁移：依赖报告的`layer`显示值由GameFoundation统一为GamePlatform；三层顺序不变，GamePlatformArena按实际MobaCommon路径分类。同步更新引擎自动化映射用例；本轮未取得UE工具链，该C++用例尚未执行，PowerShell架构回归不替代它。

本插件不提供游戏 Runtime 能力，不允许 Client Target、Server Target 或 Shipping Runtime 依赖。核心能力包括：
- `Migration/GamePlatformAssetMigrationLibrary`：Monolith/Python显式提交完整包名映射，先拒绝越界、缺源、重名和目标覆盖，再由引擎`AdvancedCopyPackages`统一重映射硬/软引用并保存。它不扫描项目业务目录、不复制第三方播放器；失败保留部分目标供审计。Python调用的bool加OutError遵循UE投影：失败返回None，成功返回错误字符串（空字符串为成功）。
- UE5.8 Data Validation（数据验证）接入。
- Naming/Dependency/InheritanceBoundary/ClientLeak/ServerLeak/Content/Definition/StableId/GameplayTag/ServerAssetSafety/RPC 静态规则。
- Asset Registry（资产注册表）驱动的资产审计。
- GamePlatformValidation Commandlet（平台验证命令行工具）。
- PR/Nightly/Release CI Gate（持续集成门禁）。
- 人工 Review Case/Report（验收用例/报告），默认 NotRun，AI 不代签。
- 统一 Plugin Review Harness（插件审核宿主）设计见 [ReviewHarness](Docs/ReviewHarness.md)；纯逻辑插件可用自动测试/Commandlet，视觉/运行插件必须在真实UE资产出现后进入人工Review。
- PerformanceTestRunner（性能测试运行器）与兼容基线判断。

默认新增业务后端接口：无。

报告统一写入 Game/Saved/GamePlatformValidation/<RunId>/，不写 Source/Content，不自动删除、重命名或移动资产。

2026-10-13开发演示证据增强（源码未构建/运行）：公开ReviewCase/Report增加独立功能/案例、完整用例合同冻结及版本化证据绑定。Private校验在构造FName前检查外部身份，拒绝路径链接/越界，核验真实state_dump与运行身份/终态/完成操作/清理结果；签收前重读原始索引、独立二进制证明及必需文件摘要。相关功能用例按自己的身份绑定，不借主功能自动通过。人工作出Passed须显式审核人及确认、当前版本/全部证据有效，决策追加写入；AI不能代签。项目Editor桥仅创建NotRun待审报告，平台无项目资源反向依赖。原生ReviewEvidence等回归只有源码，静态检查不证明通过；项目目前的unverified原始索引不能签收Passed。

三层继承源码门禁另有可复现 PowerShell 入口 `Tests/Architecture/ValidateInheritanceBoundaries.ps1`，并已接入 `ValidateDesignBaseline.ps1`；Blueprint/DataAsset真实父类仍须由UE AssetRegistry/DataValidation验证。

同次续行新增机器证明实际文件绑定：除raw/索引/测试集合，还必须绑定独立机器输入及完整内容清单文件；签收前重读摘要，拒绝导入后修改。项目工具可以显式追加机器待审索引，但并非签名信任服务，项目Editor当前内容刷新/历史/签收UI尚未闭合。本次原生测试未运行，不能把工具夹具或文本摘要当人工/UE通过。相关范围见工作空间`Docs/Implementation/PluginDemos/PluginDemoEvidenceBinding.md`。

验证状态只能依据真实证据。源码静态检查通过不等于 UE5.8 编译、DataValidation Commandlet、Cook、Shipping 或性能基线已经通过。

动画制作可显式调用`GP.Animation.InitializeDataModel <AnimSequence资产路径>`，由游戏线程使用引擎原生控制器补齐数据模型初始化前置条件，并通过`UpdateWithSkeleton`同步当前骨架的Sequencer FK骨树。仅修改Skeleton字段可能仍保留旧FK Rig，不能写入新骨骼轨道；正常骨架保留已有轨道，迁移不再存在的骨骼遵循引擎控制器清理，调用者应先备份。该入口不硬编码项目资源，不自动保存或写入关键帧；调用者随后仍须制作、保存并独立Cook。直接NewObject的序列可能缺失Sequencer MovieScene/ControlRig，工具返回帧数不等于模型有效。模块默认关闭，可在UBT用`-EnablePlugin=GamePlatformDeveloperTools`构建，在编辑器用`-EnablePlugins=GamePlatformDeveloperTools`启用；均仅Editor，不改变客户端／服务器闭包。命令随模块关闭注销，模块启动只注册入口、不修改资产。
