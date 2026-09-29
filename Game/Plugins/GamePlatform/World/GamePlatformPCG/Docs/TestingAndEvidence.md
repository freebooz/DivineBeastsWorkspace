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

## 本轮已执行验证

- `Build/Validation/VerifyPCGArchitecture.ps1（PCG架构门禁）`：**通过**。确认仅保留 Runtime+Editor 双模块、无 ProjectPCG/DivineBeastsPCG 平行身份、P0～P9 固定原语、Schema v1 关键字段、Template Contract（模板合同）和开发组件清单均存在。
- `Tests/Architecture/ValidateInheritanceBoundaries.ps1（三层继承/Public API边界）`：本轮全工作区执行**未通过**，唯一失败来自并行开发中的 `GamePlatformSettings（设置插件）`重复类型 `FSubscriptionEntry`；当前报告未指向 GamePlatformPCG。该结果只能说明 PCG 专项门禁通过，不能宣称全工作区三层门禁通过。
- UE5.8 UBT（虚幻构建工具）定向编译：第一次因相对 `.uproject` 路径无法从引擎目录解析而未进入编译；改用绝对工程路径后，UBT 返回 `ConflictingInstance`，原因是同一工作区已有另一项 `DivineBeastsArenaEditor` 构建持有全局 UBT Mutex（互斥锁）。因此当前尚无本轮 PCG 源码的最终编译通过/失败结论，不能把路径错误或互斥锁阻塞记为源码编译失败。
- 原生策略测试：使用 CMake + MSVC Release 实际构建 `PCGPolicyTests`，随后 `ctest` 执行 `PCG.Policy`，结果 **1/1 Passed，0 Failed**。该测试覆盖现有 NumericProfile（数值策略）、StableSeed（稳定种子）和 Lifecycle（生命周期）纯逻辑；不覆盖 UE 反射或新 PCG 节点 API。
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
