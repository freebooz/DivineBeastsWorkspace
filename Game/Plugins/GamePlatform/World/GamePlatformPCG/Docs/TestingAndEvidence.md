# TestingAndEvidence（测试与证据）

## 当前新增源码测试

`Private/Tests/PCGEnvironmentContractTests.cpp`覆盖：

- Schema v1字段注册与未知字段拒绝。
- P0-P9原语数量。
- Domain ID（领域ID）稳定目录。
- PriorityCarve（优先级挖洞）纯规则。
- SelectSpanMeshByLength（按跨度选网格）。
- 柱距纯规则。
- Template Contract（模板合同）ID。

原有：

- `PCGPolicyTests.cpp`：数值预算、稳定Seed、生命周期。
- `PCGEditorTests.cpp`：Source Fingerprint（源指纹）与Legacy Development Graph（旧开发图）。
- `PCGEditorTests.cpp` 同时覆盖 12 个 Foundation Template（基础模板）内存构建/合同验证与 7 个 Foundation Subgraph（公共子图）内存构建；其中专门检查 `SG_ProjectOnLandscape` 的 Landscape 输入 Pin + 官方 Projection 节点，以及 `SG_WriteClosedExclude` 的统一排除写入节点。

## 本轮已执行验证

- `Build/Validation/VerifyPCGArchitecture.ps1（PCG架构门禁）`：**通过**。确认仅保留 Runtime+Editor 双模块、无 ProjectPCG/DivineBeastsPCG 平行身份、P0～P9 固定原语、Schema v1 关键字段、Template Contract（模板合同）和开发组件清单均存在。
- `Tests/Architecture/ValidateInheritanceBoundaries.ps1（三层继承/Public API边界）`：本轮全工作区执行**未通过**，唯一失败来自并行开发中的 `GamePlatformSettings（设置插件）`重复类型 `FSubscriptionEntry`；当前报告未指向 GamePlatformPCG。该结果只能说明 PCG 专项门禁通过，不能宣称全工作区三层门禁通过。
- UE5.8 UBT（虚幻构建工具）定向编译：第一次因相对 `.uproject` 路径无法从引擎目录解析而未进入编译；改用绝对工程路径后，UBT 返回 `ConflictingInstance`，原因是同一工作区已有另一项 `DivineBeastsArenaEditor` 构建持有全局 UBT Mutex（互斥锁）。因此当前尚无本轮 PCG 源码的最终编译通过/失败结论，不能把路径错误或互斥锁阻塞记为源码编译失败。
- 原生策略测试：使用 CMake + MSVC Release 实际构建 `PCGPolicyTests`，随后 `ctest` 执行 `PCG.Policy`，结果 **1/1 Passed，0 Failed**。该测试覆盖现有 NumericProfile（数值策略）、StableSeed（稳定种子）和 Lifecycle（生命周期）纯逻辑；不覆盖 UE 反射或新 PCG 节点 API。
- 安全边界补强后，`VerifyPCGArchitecture.ps1（PCG架构门禁）`再次执行通过；新增检查包括环境 Definition 引用必须进入 `RequiredDefinitions（必需定义）`统一租约，以及 Template Contract（模板合同）运行执行在真实资产闭环前必须 Fail-Closed（失败关闭）。
- M0/M1 Foundation Template Generator（基础模板生成器）、`GamePlatformPCGFoundationTemplatesCommandlet（模板生成命令行工具）`与 `UGamePlatformPCGWorldValidator（PCG世界编辑器校验器）`源码已加入；静态门禁要求生成器设置官方 `bIsTemplate=true`、Commandlet存在、Editor模块显式依赖 DataValidation，并要求世界校验器检查唯一Director和完整参与者注册。这些Editor能力仍需定向编译、Automation及DataValidation实际调度后才能形成运行证据。
- UE5.8 定向构建已多次进入真实 UHT/编译阶段；一次任务在 `UHT processed ...` 后进入 35 个动作并开始编译 `PCGEnvironmentContractTests.cpp / GamePlatformPCGLinearRules.cpp`，随后被外部 stop request 中止；另一次进入 28 个编译动作后进程以 exit -1 结束但无 C++/UHT/Link 错误文本。目前仍不能宣称编译通过或源码编译失败，只能记录为“构建执行环境被并行任务/外部中止影响，最终结论未形成”。
- 静态审查后新增安全收敛：Template Contract 要求唯一 `SchemaWriter → SchemaValidator → Output` 可达链；Template Runtime 在真实资产/资源解析闭环前明确 Unsupported；PCG Definition 的主资产ID必须进入 `RequiredDefinitions`；MeshSet真实网格进入 `PCGGeneration` Asset Bundle；1.0 Template Profile 不再被 Legacy 单一 `OutputMesh` 强制绑定。上述项已进入源码/架构门禁，但仍等待 UE5.8 实际编译与 Automation 证据。

## 当前证据边界

本轮新代码仍需要实际补齐以下验证：

1. UHT/C++编译。
2. Runtime/Editor定向构建。
3. UE Automation。
4. Template资产创建与AssetRegistry/DataValidation。
5. Gold Level（金标准关卡）。
6. Client/Server Cook。
7. Dedicated Server纯装饰隔离。
8. 性能Profile。

在上述步骤未实际成功前，不得把对应项写为“通过”。本文件后续记录真实命令、结果和外部阻塞。

## 2026-10-10：P0/P1最新实施证据

- 已修复旧`VerifyPCG.ps1`文档真源检查清单，并增加现行中文实施计划入口；移除非必需`-NoSharedPCH`构建标志以避免额外的引擎响应文件变化。
- `VerifyPCGArchitecture.ps1`和`VerifyPCGGoldLevelPrerequisites.ps1`静态门禁返回0；CMake Debug/Release原生构建及CTest均通过，证据目录为`Saved/Validation/GamePlatformPCG/18bebe99-1d40-493c-94a1-396dc5e701d6/`。综合脚本返回2表示整体UE验收仍缺失，不能写成全通过。
- Editor Win64 Development构建尝试在UBT出现4178/4179项引擎级动作，两个Job均经调用者主动停止；无完整UE编译结论。UBT日志位于`Saved/Validation/GamePlatformPCG/ca5782fa-8925-498e-81a6-84c302bb1696/`及`6409243c-b792-4def-8473-d1ef577f6239/`。
- `UnrealEditor-Cmd -run=GamePlatformPCGFoundationTemplates`实际启动后，因完整工程缺失`GamePlatformCameraClient`模块而在插件加载阶段失败；`Game/Saved/Logs/DivineBeastsArena.log`记录`LogPluginManager: Error`。目前Foundation模板/子图实际落盘数0。
- 本次未执行UE Automation、Gold Level地图、AssetRegistry/DataValidation、独立关闭重开、Client/Server Cook、专服剥离及性能测试。P1为外部模块装配阻断，P2～P7尚未启动，不能宣称PCG生产链已完成。

## 2026-10-10：继续实施P1元数据校验（补充检查点）

- `AssignMeshSet`已通过模块内`GamePlatformPCGNodeMetadata::AssignMeshSetId`明确逐点赋值，避免已有Schema默认空ID导致下游网格目录丢失；`ValidateSchema`增加核心必需、已注册属性类型、未知`Pcg.*`及未经批准Guid字段的失败关闭。源码对应`Private/Nodes/GamePlatformPCGNodes.cpp`，新私有合同为`Private/Nodes/GamePlatformPCGNodeMetadata.h`。
- UE Automation源码新增`GamePlatform.PCG.Metadata.AssignMeshSetToPoints`与`GamePlatform.PCG.Schema.MetadataTypes`，**尚未编译或运行，不能写“通过”**；专项架构门禁、Gold Level前置门禁和`git diff --check`已返回0。
- 最新原生CMake Debug/Release编译与CTest均通过，证据`Saved/Validation/GamePlatformPCG/1dd443f1-64b0-48f4-80f0-310a645508d1/`；这些测试不覆盖本轮UE PCG点数据行为。
- 单模块`GamePlatformCameraClient`（相机客户端）UBT编译先遇到外部`ConflictingInstance`（互斥锁冲突，退出码10）；重试并加`-NoEngineChanges`后因必须更新已有引擎文件而返回`FailedDueToEngineChange`（退出码5），详见`Saved/Validation/GamePlatformPCG/CameraClient_ModuleBuild_20261010_retry.log`。当前缺8个实际编辑器DLL；在正式构建环境修复前，不能伪造UE运行证据。
- 新Foundation模板/子图资产数量仍为0；独立Editor关闭重开、AssetRegistry/DataValidation（资产注册/数据校验）、G01～G16、Client/Server Cook、三世界权威一致性与性能Profile（性能实测）仍待执行。P1为代码局部已修复、引擎验收受阻；P2～P7未取得生产准入。
## 2026-10-10｜P2～P7统一测试回执（代码优先）

- 静态：`VerifyPCGArchitecture.ps1`和`VerifyPCGGoldLevelPrerequisites.ps1`均返回0；它们只核查源码结构与前置合同。
- 原生：`VerifyPCG.ps1 -NativeTests`中的文档、CMake Debug/Release编译及CTest全部子项返回0，证据`Saved/Validation/GamePlatformPCG/7e4b497f-8c3e-4f24-aa89-62fafb6d8d5f/`，总码2按约定表示整体验收未完成。新增高级空间及存档边界的UE测试源码**未被该原生目标编译或运行**。
- UE5.8 Editor定向模块构建：UHT解析`DBAWorldsRuntime/Public/Gameplay/DivineBeastsWorldGameMode.h(37)`时报`Invalid use of keyword 'private'`（退出码6），在进入PCG C++编译前失败。日志`Saved/Validation/GamePlatformPCG/PCG_P2P7_EditorModules_20261010.log`；该文件属于并行世界/天气修改，未由PCG任务覆盖。
- 真实UE Blueprint、PCG模板/子图、DataAsset和GoldLevel世界地图还没有成功生成并完成关闭重开；Server Cook、多人状态恢复、HiGen/HLOD以及可视化性能/人工验收均未执行。
- 本任务只确认P2～P7核心源码与Blueprint生成工具的落盘，不宣布每项计划已经实现可投入生产的运行闭环。
## 2026-10-10｜UHT恢复及PCG C++检查补充证据

- 修复`DivineBeastsWorldGameMode.h`中拼接的include后，UBT不再报原先的`private` UHT语法错误；定向UBT因`-NoEngineChanges`触发`FailedDueToEngineChange`（5）而停止，日志`Saved/Validation/GamePlatformPCG/PCG_P2P7_EditorModules_20261010_recheck.log`。该阻断属于正式引擎构建链，不是PCG源文件的编译诊断。
- MSVC 14.44`/Zs`基于UBT真实响应文件完成PCG 26份中的前18份语法检查（均退出0），余下8份因同时存在其它正式构建主动停止重复检查；**未编译的8份不可标记通过**。并行正式UBT日志显示PCG Runtime及Editor Unity C++编译推进至链接，但因`LNK1181`缺引擎导入库未完成正式构建。
- 本次静态架构和金标准前置检查返回0；原生策略测试六项通过，证据`Saved/Validation/GamePlatformPCG/762e800a-fea7-4906-a044-6e5e6c08cbe3/`，综合总码2表示实际UE验收仍缺失。
- 正式Foundation/PCG蓝图/验证地图`.uasset`/ `.umap`新增数均为0，UE Automation、Editor独立重开、GoldLevel、Client/Server Cook、专服权威/性能/人工验收仍未执行。不得因旧编译日期DLL存在而把新增蓝图创作接口当成已运行。
