# GamePlatform 设计审查修复执行计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. 本次用户已明确要求生成计划后连续执行并提交远程；无需再次审批计划。独立领域采用 superpowers:dispatching-parallel-agents，所有提交/推送由主执行者统一完成。步骤使用勾选框追踪。

**Goal:** 修复审查发现的现存插件缺陷，补齐真实验证及中文说明，并将可审查的源码提交推送到远程修复分支。

**Architecture:** 保留 GamePlatform → MobaCommon → DivineBeasts 的复用关系以及各领域现有作用域。按无重叠文件责任并行修复应用/玩家、世界/玩法和表现；主执行者负责基础数据、服务器、依赖、全局门禁及共享接口整合。

**Tech Stack:** 锁定 UE5.8、C++/GAS/CommonUI/Niagara、PowerShell 7、Pester、现有 Native C++ 测试和 Git。

**Spec:** `Docs/Implementation/GamePlatformDesignRemediation/ReviewSpecification.md`。

## Global Constraints

- 唯一正式工程为 `Game/DivineBeastsArena.uproject`；插件源码只维护在 `Game/Plugins`。
- 40 个 GamePlatform 稳定身份、46 个代码/机制插件及真实内容 N 保持现行基线；不恢复 GamePlatformOpenWorld，不新增大厅角色。
- 三层单向依赖、同层无环；客户端/服务器私有模块不互相依赖；纯表现不进入专服产物。
- 不更名稳定模块、反射类型、资产、协议或发布字段；需要兼容迁移时先列影响、旧消费者与回退。
- 所有修改的一方代码、配置、测试和接口必须具备同步中文说明。
- 不伪造 UE 资产、测试日志、后端实现或成功结果；不执行生产支付/退款/发布。
- 根目录规划与受影响插件说明同步；分支 `codex/gameplatform-design-remediation` 从 `6f25a6519c9cc3850dd612bd073a3d79c0ba130a` 开始。
- 用户已授权提交远程；只推送本修复分支，不强推、不替换他人提交。

## Review Focus

- 回调触发关闭或切换作用域后，旧操作不会读新作用域或已释放对象。
- 相同关联ID、旧超时、重复停止及先取消后迟到事件不覆盖新操作，不丢终态。
- 有限队列满、版本冲突、加载失败保留已接受数据并明确返回失败。
- 页面暂时失活/组件销毁/世界关闭遵循各自所有权，资源只撤销本调用方。
- 未接入真实后端、资产或UE运行的能力不以静态门禁或模块壳宣告交付。

---

### Task 1: 执行基线与规格登记

**Files:** 新建本计划、`Docs/Implementation/GamePlatformDesignRemediation/ReviewSpecification.md`、`ExecutionProgress.md`；修改总体目录规划说明。
**Interfaces:** Consumes 审查 F01—F30；Produces 固定基线、文件归属、执行账本。
- [x] 核对工作树、当前版本、远程、规则和真实引擎；创建原生隔离工作树。
- [x] 重跑现有设计基线：预期发现尚未修复的18条重复装配诊断，而不是假定干净基线。
- [x] 登记新增文档与各领域任务；账本保留每项结论和测试证据。

### Task 2: 应用与玩家服务正确性

**Files:** `Game/Plugins/GamePlatform/Application/{GamePlatformInput,GamePlatformLoading,GamePlatformSave,GamePlatformSettings}/` 和 `PlayerServices/{GamePlatformEquipment,GamePlatformEntitlement,GamePlatformProgression,GamePlatformLiveOps,GamePlatformCommerceUI}/` 内现存源码、Docs和Private/Tests；不修改全局目录规划。
**Interfaces:** Consumes 现有 Online 服务公开合同及 GAS 授予句柄；Produces 保持现有调用身份的终态/快照行为。新增共享接口先向主执行者报告，不访问其他模块 Private。
- [x] 重核 F01/F02/F06/F07/F13/F14/F20/F28/F29，在账本记录已被其他提交消除的项。
- [x] 先补真实回归：HTTP弱引用终结；订阅关闭重入；装备组件销毁且ASC存活；输入只重置自有映射和真实落盘失败；A/B用户交错保存；超大存档；首次/清空/同Revision时间边界事件。
- [x] 使用现有Native测试入口观察可运行策略测试失败；引擎专属用例放Private/Tests并记录等待UE执行。
- [x] 修复请求引用、注册注销、持久化用户上下文、读取上限和事件通知；四服务优先复用统一Online，保持失效回调和登录退出保护。
- [x] 同步API/README真实完成度与中文注释，返回新增文件清单和准确验证命令。

### Task 3: 世界与玩法生命周期

**Files:** `World/{GamePlatformNavigation,GamePlatformPCG,GamePlatformInteraction}/`、`Gameplay/{GamePlatformQuest,GamePlatformGameplay,GamePlatformAI,GamePlatformAbilitySystem,GamePlatformCharacter}/` 内现存源码、Docs和Private/Tests。
**Interfaces:** Consumes 现有Data服务、Gameplay资格与GAS合同；Produces 原调用ID和发布类型兼容的查询/任务/出生行为。F16 Data共享接口由Task5主执行者拥有，若需新增先协调。
- [x] 重核 F03/F04/F15-PCG/F16-CharacterAI/F22/F23/F26/F27。
- [x] 先补重复导航ID/旧超时、Quest满队列对账与加载失败/同Revision进度、129次顺序PCG、延迟Possess、Editor世界不生成控制器、自定义出生偏移、Gate缺失/旧Avatar的回归。
- [x] 请求调度前拒绝重复在飞ID；终结PCG记录有界；Quest快照验证/重放按事务提交并保留失败数据。
- [x] 接通实际激活资格检查，拥有关系变化驱动交互，出生使用已验证变换；AI过滤真实Game/PIE权威世界。
- [x] 保留已发布资产身份并收口Character/AI资源租约；无法直接转换的旧接口明确兼容入口及真实消费者迁移。
- [x] 同步中文合同、Quest集成真实状态和测试证据；不修改审查期间新增的无关Combat规则。

### Task 4: 表现解析与实例所有权

**Files:** `Presentation/{GamePlatformPresentation,GamePlatformUI,GamePlatformVFX,GamePlatformSFX,GamePlatformSurface}/` 内源码、Docs和Private/Tests。所有.uplugin由Task5统一负责。
**Interfaces:** Consumes Data租约、中立请求与CommonUI容器合同；Produces 精确优先/显式回退/冲突失败，以及作用域代次隔离的表现终态。
- [x] 重核 F08/F09/F10/F11/F12/F16-UI/F17/F21；不重复修改已登记的Composite步骤定时器清理。
- [x] 先补精确语义vs高Scope父语义、同局部EntryId不同fragment、附着目标销毁、Steps A↔B环、SFX启动拒绝及总预算、A→B→A、重复通知ID、预测结束后确认/先取消后请求、MPC配置更换回归。
- [x] 实现精确优先与显式逐级回退；跨注册身份歧义必须失败；弱附着目标逐次验证世界。
- [x] Steps真实依赖进入统一预检图；音频失败清理和Pending+Active总容量守恒；UI区分显示激活和最终移除。
- [x] 有界终态/取消记录阻断迟到重播；Surface刷新解析新MPC；UI加载按Task5约定迁入Data兼容资源合同。
- [x] 同步中文说明和验证证据，不创建/修改UI视觉资产。

### Task 5: 基础数据、服务器、装配与门禁

**Files:** `Foundation/GamePlatformData/`、`OnlineServices/GamePlatformServer/`、`Diagnostics/{GamePlatformTelemetry,GamePlatformDebug}/`、VFX/DBAWorlds/DBAArena描述文件、现有 `Tests/Architecture`。
**Interfaces:** Consumes Task2—4共享接口需求；Produces Data合法旧租约幂等/终态边界、HTTPProvider Shutdown、正确插件闭包与函数范围遥测检查。
- [x] F18 使用现有失败装配回归补实际插件依赖，保留各目标边界。
- [x] F05/F24 补Provider关闭/取消在飞请求及接收阶段限长回归，统一终态解绑后销毁。
- [x] F15-Data 采用能证明旧签发句柄真实性且有界的终态策略，同步Release/GetLeaseState合同；不能将伪造GUID当已释放。
- [x] F16 明确普通软资源与Definition的中央租约入口及兼容路径，保持唯一数据所有者，不改变发布PrimaryAssetId；与Task3/4消费者接线。
- [x] F25 用旧全文件误报用例与真实危险捕获用例验证扫描范围，再修复脚本；不通过删掉检测解决。
- [x] 运行Native/Data/Server/Telemetry相关支持的检查，并记录哪些只属静态证据。

### Task 6: 成熟度与全局中文文档收口

**Files:** 根总体规划/目录规划/三层实施/插件清单、相关README和执行账本。
**Interfaces:** Consumes 各任务实际修改与检查；Produces F19/F20/F30真实能力矩阵和迁移/回退记录。
- [x] 五个预留插件明确未实施、现有已批准计划和独立职责门禁；不以缺陷修复为名实现无需求空框架或擅自退休身份。
- [x] 删除失真的后端/迁移/Outbox交付宣称，修正Session/SFX旧状态；未接入能力列剩余项。
- [x] 每个新增/修改文件审阅中文责任、端侧、调用方、失败、取消与所有权说明；登记全部新增文件的目录归属。
- [x] 每个F编号记录“已修复/原问题已消除/兼容迁移落实/成熟度收口/未完成及原因”，不能把文档修订称为后端能力完成。

### Task 7: 整体验证、复核和远程提交

**Files:** 现有Build/Game和Validation入口；证据仅写Saved，正式摘要写ExecutionProgress。
**Interfaces:** Consumes 集成后的工作树；Produces 实际退出码、独立代码复核、提交和远程分支。
- [x] 最新架构基线、继承边界、Pester回归、头文件审计与专项门禁通过；失败先定位并修复，不能降低版本或关闭模块。
- [x] 使用已存在脚本和UE5.8执行受影响Editor/Client/Server构建与相关Automation，记录日志/退出码；无法执行的环境或依赖明确列出。
- [x] 复核全部差异、公开边界、中文说明和实际测试范围；重要问题修复后重跑覆盖用例。
- [x] 精确暂存本分支变更，提交后推送 `origin codex/gameplatform-design-remediation`，核对远程提交一致。不自动合并main或发布生产。
- [x] 最终报告提交号、远程链接、已执行检查、未执行项、成熟度缺口和回退方式。


执行边界：上述构建项表示已实际执行并记录结果；Client/Server选定模块对象编译通过，Editor构建退出6归锁定引擎既有HTTP测试，Automation/Cook/联机尚未验收。不得把勾选解释为全部UE动态检查通过。
