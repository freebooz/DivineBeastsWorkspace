# GamePlatformDeveloperTools（游戏平台开发者工具插件）

定位：GamePlatform/Diagnostics（游戏平台层／诊断分类）的 Editor/CI 专用插件。

2026-09-27目录迁移：依赖报告的`layer`显示值由GameFoundation统一为GamePlatform；三层顺序不变，GamePlatformArena按实际MobaCommon路径分类。同步更新引擎自动化映射用例；本轮未取得UE工具链，该C++用例尚未执行，PowerShell架构回归不替代它。

本插件不提供游戏 Runtime 能力，不允许 Client Target、Server Target 或 Shipping Runtime 依赖。核心能力包括：
- UE5.8 Data Validation（数据验证）接入。
- Naming/Dependency/ClientLeak/ServerLeak/Content/Definition/StableId/GameplayTag/ServerAssetSafety/RPC 静态规则。
- Asset Registry（资产注册表）驱动的资产审计。
- GamePlatformValidation Commandlet（平台验证命令行工具）。
- PR/Nightly/Release CI Gate（持续集成门禁）。
- 人工 Review Case/Report（验收用例/报告），默认 NotRun，AI 不代签。
- PerformanceTestRunner（性能测试运行器）与兼容基线判断。

默认新增业务后端接口：无。

报告统一写入 Game/Saved/GamePlatformValidation/<RunId>/，不写 Source/Content，不自动删除、重命名或移动资产。

验证状态只能依据真实证据。源码静态检查通过不等于 UE5.8 编译、DataValidation Commandlet、Cook、Shipping 或性能基线已经通过。
