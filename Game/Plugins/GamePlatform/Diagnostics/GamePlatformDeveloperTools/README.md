# GamePlatformDeveloperTools（游戏平台开发者工具插件）

定位：GamePlatform/Diagnostics（游戏平台层／诊断分类）的 Editor/CI 专用插件。

2026-09-27目录迁移：依赖报告的`layer`显示值由GameFoundation统一为GamePlatform；三层顺序不变，GamePlatformArena按实际MobaCommon路径分类。同步更新引擎自动化映射用例；本轮未取得UE工具链，该C++用例尚未执行，PowerShell架构回归不替代它。

本插件不提供游戏 Runtime 能力，不允许 Client Target、Server Target 或 Shipping Runtime 依赖。核心能力包括：
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

三层继承源码门禁另有可复现 PowerShell 入口 `Tests/Architecture/ValidateInheritanceBoundaries.ps1`，并已接入 `ValidateDesignBaseline.ps1`；Blueprint/DataAsset真实父类仍须由UE AssetRegistry/DataValidation验证。

验证状态只能依据真实证据。源码静态检查通过不等于 UE5.8 编译、DataValidation Commandlet、Cook、Shipping 或性能基线已经通过。

动画制作可显式调用`GP.Animation.InitializeDataModel <AnimSequence资产路径>`，由游戏线程使用引擎原生控制器补齐数据模型初始化前置条件。该入口不硬编码项目资源，不自动保存或写入关键帧；调用者随后仍须制作、保存并独立Cook。直接NewObject的序列可能缺失Sequencer MovieScene/ControlRig，工具返回帧数不等于模型有效。模块默认关闭，可在UBT用`-EnablePlugin=GamePlatformDeveloperTools`构建，在编辑器用`-EnablePlugins=GamePlatformDeveloperTools`启用；均仅Editor，不改变客户端／服务器闭包。命令随模块关闭注销，模块启动只注册入口、不修改资产。
