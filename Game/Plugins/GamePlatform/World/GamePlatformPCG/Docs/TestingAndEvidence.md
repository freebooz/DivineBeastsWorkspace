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
