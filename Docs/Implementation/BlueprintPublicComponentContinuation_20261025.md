# SSLM 公共组件基类优先优化实施记录（2026-10-25续行）

> 正式工程：`DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`（神兽联盟工程）。本轮通用能力约束已补充到 [《三层类继承与扩展规范》](../Architecture/三层类继承与扩展规范.md) 第九章，此文仅记录执行，不建立竞争规范。

## 一、本轮实际修改

- 平台现有 `UGamePlatformSlotWidget`（通用槽位控件基类）增加 `FGamePlatformUISlotCellPresentation`（槽位展示数据）、`AreSlotStatesEquivalent`（展示值比较）、`ApplyNamedSlotCellPresentation`（命名控件呈现）。复用真实的已有公共组件，不另建第二个槽位基类。
- 项目 `FDivineBeastsInventoryCellPresenter`（背包格展示适配器）改为平台展示值别名；文字、按钮可用性、工具提示和背景投影由平台通用接口处理。项目只持有中文物品说明、选中配色和既有布局约定。正式背包父类、命名控件及服务器业务操作不变。
- `FDivineBeastsAbilityBarProjectionDiff`（技能栏差异比较）改为复用公共比较。技能槽保留不显示 Count（数量）的特殊比较策略；不动GAS（游戏技能系统）授权、技能激活或冷却权威数据。
- 新增 `FGamePlatformUISnapshotScope`（界面来源快照校验值契约），由平台 `UGamePlatformPartyRosterWidget`（队伍控件）、`UGamePlatformCombatAlertWidget`（战斗警告）以及项目 `UDivineBeastsScopedDomainViewModelBase`（领域来源视图模型基类）共同使用。此类无状态跨继承领域规则采用组合，不创建独立 UObject、Actor 或 Subsystem（对象、场景对象或子系统）。
- 新增 `GamePlatformUISlotCommonPresentationTests.cpp`（平台槽位与来源规则自动化测试）和 `Tests/Architecture/ValidateSharedUIComponentReuse.ps1`（通用组件复用静态门禁）。

## 二、继承与组合裁定

- **稳定实体、控件与展示状态生命周期：** 优先使用平台公共基类，例如 `UGamePlatformComponentWidget`、`UGamePlatformSlotWidget`、`UGamePlatformSlotBarWidget`、`UGamePlatformViewModelBase`、`UGamePlatformAbilitySystemComponent`（公共UI组件、槽位、槽位栏、视图模型及能力系统组件）。
- **互不相干继承树的共同约束：** 用可组合策略或接口，例如来源作用域校验。项目特有背景颜色、角色名字、技能授权等不能塞回平台。
- **纯资产差异：** 使用 DataAsset、Definition、Material Instance（数据资产、定义、材质实例），不为每个生肖技能、湖泊、Niagara效果生成没有业务职责的类。
- **真正的蓝图父类迁移：** 两份历史 `WBP_DBA_UI_AbilitySlot`（技能槽）在 Monolith 读取真实父类、硬软引用、重载和Cook完成前继续保留，禁止用文本或脚本伪造 UE 二进制资产。

## 三、工程验证证据

- 平台模块 `GamePlatformUIClient`（通用UI客户端模块）Client定向构建通过（exit 0），证据：`Saved/Validation/FoundationM0/072816e7-f1a6-4de2-a821-581ff54ddfb3/Build`。
- 项目模块 `DivineBeastsUIClient`（神兽联盟UI客户端模块）Client定向构建通过（exit 0），证据：`Saved/Validation/FoundationM0/c7bd2755-790d-417e-bd67-aa9c44e48d1e/Build`。
- 平台Editor（编辑器）模块所需的7个原生编译步骤中，所有源文件编译成功，包括新增自动化测试；链接 `UnrealEditor-GamePlatformUIClient.dll` 时出现 `LNK1104` 文件占用，导致编辑器模块**未构建成功**。证据：`Saved/Validation/FoundationM0/07444040-dd09-4ad9-8fee-1c0702d53123/Build`。不关闭他人编辑器或破坏并行任务。
- 三层公开类型继承门禁实测通过：587个Public（公开）头文件、1,275个类型和212条继承边；`Tests/Architecture/ValidateSharedUIComponentReuse.ps1`（公共组件复用静态门禁）实际执行14项检查通过。仍未执行Editor原生自动化用例、Monolith资源编译或冷重载，不能把编译成功当成用例通过。
- 本轮无 `.uasset`、`.umap` 修改，无Git Commit/Push（版本提交/推送）。构建通过仅指受影响模块，不代表整个客户端、服务器或发行Cook通过。

## 四、下一批实施顺序

1. 在Editor DLL不再被其他任务使用时，补做编辑器链接、实际执行新平台测试及既有项目背包/技能栏/组队UI回归。
2. 恢复Monolith实时服务，读取全部真正的控件父类、Widget树与动态资源引用，再逐步将单格背包/技能槽迁移到共用作者Widget及平台控件基类；无原生资源验收不删除历史蓝图。
3. 继续审查GamePlatformCharacter/Combat/Water/PCG、Camera、VFX、SFX等已有抽象，优先复用真实通用Component（组件）、Interface（接口）及DataAsset（数据资产）；只有出现稳定新合同和真实多消费者才增加新基类。
4. 对每次变动记录源码、原生资产、目标构建、联机、Cook和人工签收各自状态，不把计划或静态检查冒充完成。

## 五、并行版本状态记录

本任务启动时 `main`（主分支）HEAD为 `63e5a72d`，执行中观察到并行工作流继续推进为 `071ba9a5`、`e5b2cfa6`。本执行没有调用 `git commit`（版本提交）或 `git push`（远程推送）；收口检查时，平台/项目重构源码已与当前HEAD一致，执行计划仍有未提交变更，本续行报告与静态复用门禁脚本仍为未跟踪文件。后续交付时应按明确路径核对并行提交，不能将当前分支其他变化冒充本次完成。
