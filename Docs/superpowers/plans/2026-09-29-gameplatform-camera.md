# GamePlatformCamera 实施执行计划

> **执行前置技能：** REQUIRED SUB-SKILL：默认使用 superpowers:executing-plans 逐任务实施；只有用户明确选择委派执行时，才使用 superpowers:subagent-driven-development。所有步骤使用 `- [ ]` 复选框跟踪，未取得对应真实证据不得勾选。

**Goal:** 将 GamePlatformCamera 从未接入的 ClientOnly 模块壳实现为 LocalPlayer 作用域、Definition 驱动、可由 DivineBeasts 输入与世界流程真实消费的通用客户端相机服务，并形成自动化、Review、Cook 和服务器剥离证据。

**Architecture:** 保留单一 GamePlatformCameraClient 模块；LocalPlayerSubsystem 拥有请求账本、世界代次和 GamePlatformData 租约，私有 CameraModifier 复用 PlayerCameraManager、CameraShake 和 SpringArm。持续模式、Look 输入和瞬时效果使用独立合同；GamePlatformPresentation 只适配瞬时／定时 Camera 请求，DBAClient 只负责输入与已存在的 InWorld 事实组合。

**Tech Stack:** Unreal Engine 5.8 C++、UBT/UHT、Unreal Automation、GamePlatformData、GamePlatformPresentation、UE PlayerCameraManager／CameraModifier／CameraShake／SpringArm、PowerShell 7、Unreal Python、Pester。

**Spec:** Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ImplementationSpecification.md

## 全局约束（Global Constraints）

- 只保留 GamePlatformCameraClient — ClientOnly 一个模块；不新增 Runtime、Core、Server 或 Editor 空模块。
- Public 依赖只允许 Core、CoreUObject、Engine、GameplayTags、GamePlatformCore、GamePlatformData；Private 只增加 GamePlatformPresentationCore、GamePlatformPresentationClient。
- GamePlatformCamera 不得依赖 GamePlatformInput、GamePlatformUI、GamePlatformSettings、GameplayCameras、MobaCommon、DivineBeasts 或 DBAArena。
- 所有 Camera Definition 继续使用统一 GamePlatformDefinition 主资产类型；模式逻辑身份使用 platform.camera.mode 命名空间，效果使用 platform.camera.effect，并通过 ExpectedClass 区分。
- 每个 LocalPlayer 最多 64 个 Pending＋Active＋Suppressed 模式请求、64 个 Pending＋Active 效果请求和 128 条终态墓碑。
- 模式选择固定为 Definition.Priority 降序，再按 ActivationSequence 降序；Pending 不参与选择。
- LookDelta 不乘 DeltaSeconds；LookRate 只在 Camera 最终消费端乘一次 DeltaSeconds。
- 每帧只允许一个 CameraModifier 路径；不得在 Tick 中加载资产、重排全栈、扫描目录、轮询 UI、发送网络请求或分配无界容器。
- Camera 完全客户端，不声明 RPC、复制属性、命中、伤害、服务器目标选择或后端调用。
- 所有人工维护的 C++、Build.cs、PowerShell、Python、配置、测试和文档必须具备职责充分且与实现同步的中文说明。
- .uasset／.umap 只能由锁定 UE5.8 编辑器生成；不得写文本占位、改扩展名或以包头检查冒充加载、Cook、Review 通过。
- Saved/Validation 中的证据不得提交；测试、编译、Cook、Review 和人工确认分别报告。
- 保护当前工作区无关改动；每次提交只暂存任务列出的文件，不使用 reset、checkout 或批量清理。
- 当前正式目标只有 DivineBeastsArenaEditor、DivineBeastsArenaClient 和 DivineBeastsArenaServer；仓库不存在 DivineBeastsArena 普通 Game Target。计划分别验证 Editor／Client／Server Development 与 Client Shipping；不得虚构普通 Game Target 已验证，若后续新增该目标必须单列构建证据。
- 当前没有正式的项目锁定目标、死亡、观战客户端事实合同；一期不得伪造这些生产事实。平台 API、Automation 和真实 Review 场景证明能力，后续在事实合同出现后单独接入。

## 审查重点（Review Focus）

- Definition 在世界切换后迟到完成：旧请求必须 Cancelled，不能激活新世界模式；由 Task 4 的 GamePlatform.Camera.Service.StaleCompletion 锁定。
- 相同优先级的 Definition 异步乱序完成：只按成功激活时签发的 ActivationSequence 决胜；由 Task 3 的 GamePlatform.Camera.Policy.AsyncActivationOrder 锁定。
- 同一 PlayerController 的 CameraManager 被替换但没有 ControllerChanged：下一次 Look／模式／效果请求必须 RefreshBinding 到新 Manager，不得继续操作旧 Manager；由 Task 5 的 GamePlatform.Camera.Engine.ManagerReplacement 锁定。
- TargetProvider 返回其他世界或非有限坐标：不得污染当前 POV；按激活必需条件或 TargetLossPolicy 失败／回退；由 Task 5 的 GamePlatform.Camera.Engine.InvalidTargetSnapshot 锁定。
- Presentation 收到畸形 DefinitionId、Persistent、重复 Confirmed 或墓碑淘汰后的 Cancelled：必须拒绝、去重或返回 Unknown，不得重复播放；由 Task 6 的 GamePlatform.Camera.Presentation.InvalidAndEvictedRequests 锁定。

---

## 文件结构（File Structure）

### 平台公开合同

- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Definitions/GamePlatformCameraModeDefinition.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Definitions/GamePlatformCameraEffectDefinition.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Interfaces/IGamePlatformCameraService.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Interfaces/IGamePlatformCameraTargetProvider.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Subsystems/GamePlatformCameraLocalPlayerSubsystem.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraHandles.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraRequests.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraSnapshot.h
- Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraTargetSnapshot.h

### 平台私有实现

- Private/Definitions：Definition 校验和 Asset Bundle 声明。
- Private/Modes：固定容量模式栈和过渡状态机。
- Private/Loading：GamePlatformData 适配、异步租约和取消。
- Private/Bindings：PlayerController／PlayerCameraManager 绑定。
- Private/Modifiers：唯一每帧 CameraModifier。
- Private/Execution：值语义镜头评估和碰撞查询适配。
- Private/Effects：CameraShake 请求账本和原生执行。
- Private/Presentation：中立 Presentation 请求适配。
- Private/Diagnostics：无敏感身份的统计和 Unreal Insights／CSV 声明。
- Private/Tests：合同、策略、生命周期、引擎、效果和 Presentation Automation。

### 项目组合接入

- DivineBeastsInputClient 只把 Look 交给 Camera 服务；TargetLock 仍交给项目目标系统。
- DivineBeastsPresentationClient 内新增私有 LocalPlayer Camera 组合子系统，订阅现有 ApplicationFlow InWorld 事实并持有基础世界模式句柄。
- DBAClient.uplugin 显式依赖 GamePlatformCamera；Server Target 显式禁用 Camera。

### 资产与验证

- Tools/AssetTools/CreateGamePlatformCameraReviewAssets.py：只在 UE 编辑器内生成平台中立 Definition、Shake 和 Review Map。
- Tests/Camera/Assets/test_create_gameplatform_camera_review_assets.py：脱离 UE 的脚本行为测试。
- GamePlatformCamera/Tests/Scripts：静态架构、Client Cook 和 Server Stage 审计。
- Build/Validation/VerifyCamera.ps1：统一编排静态检查、目标构建、Automation、Cook 和产物审计，保留真实退出码。

---

### Task 1：锁定模块、目标与验证边界

**Files:**

- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/GamePlatformCamera.uplugin
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/GamePlatformCameraClient.Build.cs
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/GamePlatformCameraClient.cpp
- Modify: Game/Source/DivineBeastsArenaServer.Target.cs
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/TestGamePlatformCameraArchitecture.ps1
- Create: Build/Validation/VerifyCamera.ps1

**Interfaces:**

- Consumes: GamePlatformData、GamePlatformPresentation 的现有插件描述与模块名；Build/Game/FoundationTools.psm1 的证据和进程函数。
- Produces: 单模块／依赖／服务器禁用静态合同；VerifyCamera.ps1 参数 Static、BuildEditor、BuildClient、BuildClientShipping、BuildServer、Automation、AutomationFilter、CookClient、CookServer、ServerStageDirectory、EngineRoot、RunId。

- [ ] **Step 1: Write the failing architecture assertions**

在 TestGamePlatformCameraArchitecture.ps1 中断言：

    descriptor.Modules == [{ Name: GamePlatformCameraClient, Type: ClientOnly }]
    descriptor.CanContainContent == true and EnabledByDefault == false
    descriptor.Plugins contains GamePlatformData and GamePlatformPresentation
    Build.cs public dependencies equal the approved public set
    Build.cs private dependencies equal the approved private set
    Server.Target.cs contains DisablePlugins.Add("GamePlatformCamera")
    Camera source contains no MobaCommon, DivineBeasts, GamePlatformInput,
        GamePlatformUI, GamePlatformSettings, GameplayCameras, RPC or GWorld

- [ ] **Step 2: Run the static test and verify RED**

Run:

    & ./Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/TestGamePlatformCameraArchitecture.ps1

Expected: exit 1，至少报告缺少 GamePlatformData／GamePlatformPresentation 插件依赖、Build.cs 依赖和 Server 显式禁用。

- [ ] **Step 3: Apply the minimal module and target declarations**

修改 uplugin、Build.cs 和 Server Target；GamePlatformCameraClient.cpp 增加中文文件职责说明，但 StartupModule 不登录、不加载地图、不创建玩家。VerifyCamera.ps1 使用 FoundationTools 创建 Saved/Validation/Camera/<RunId> 证据，默认只运行静态检查；任何构建、Automation 或 Cook 都必须显式开关。Editor／Client／Server Development 可调用既有 BuildFoundation.ps1；Client Shipping 必须通过 FoundationTools 的受控进程入口调用锁定 UBT，并在结果中单独记录 Target、Platform、Configuration 和退出码，不得复用 Development 成功状态。

- [ ] **Step 4: Run the architecture test and design baseline**

Run:

    & ./Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/TestGamePlatformCameraArchitecture.ps1
    Invoke-Pester -Path ./Tests/Architecture/PluginModuleGraph.Tests.ps1 -Output Detailed

Expected: Camera 专项脚本 exit 0；模块图测试全部通过。输出必须声明静态结果不替代 UE 编译。

- [ ] **Step 5: Commit the boundary**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/GamePlatformCamera.uplugin
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/GamePlatformCameraClient.Build.cs
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/GamePlatformCameraClient.cpp
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/TestGamePlatformCameraArchitecture.ps1
    git add Game/Source/DivineBeastsArenaServer.Target.cs
    git add Build/Validation/VerifyCamera.ps1
    git commit -m "build: lock GamePlatformCamera client boundaries"

### Task 2：增加公开合同与可验证 Definition

**Files:**

- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraHandles.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraRequests.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraSnapshot.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Types/GamePlatformCameraTargetSnapshot.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Interfaces/IGamePlatformCameraService.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Interfaces/IGamePlatformCameraTargetProvider.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Definitions/GamePlatformCameraModeDefinition.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Definitions/GamePlatformCameraEffectDefinition.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Definitions/GamePlatformCameraModeDefinition.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Definitions/GamePlatformCameraEffectDefinition.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraContractTests.cpp

**Interfaces:**

- Consumes: FGamePlatformResult、FGamePlatformId、UGamePlatformDefinitionBase、FPrimaryAssetId、UCameraShakeBase。
- Produces:
  - FGamePlatformCameraModeHandle 与 FGamePlatformCameraEffectHandle。
  - FGamePlatformCameraModeRequest、FGamePlatformCameraEffectRequest、FGamePlatformCameraLookInput。
  - FGamePlatformCameraSnapshot 与 FGamePlatformCameraDiagnostics。
  - IGamePlatformCameraTargetProvider::TryBuildCameraTargetSnapshot(FGamePlatformCameraTargetSnapshot&) const。
  - IGamePlatformCameraService::Get(ULocalPlayer&)。
  - AcquireMode、ReleaseMode、GetModeState、SubmitLookInput、RequestEffect、StopEffect、GetSnapshot、GetDiagnostics。
  - UGamePlatformCameraModeDefinition 与 UGamePlatformCameraEffectDefinition。

公开异步操作签名固定为：

    FGamePlatformCameraModeHandle AcquireMode(
        const FGamePlatformCameraModeRequest& Request,
        FGamePlatformCameraModeCompletion Completion,
        FGamePlatformResult& OutResult);
    FGamePlatformResult ReleaseMode(
        const FGamePlatformCameraModeHandle& Handle);
    FGamePlatformResult SubmitLookInput(
        const FGamePlatformCameraLookInput& Input);
    FGamePlatformCameraEffectHandle RequestEffect(
        const FGamePlatformCameraEffectRequest& Request,
        FGamePlatformCameraEffectCompletion Completion,
        FGamePlatformResult& OutResult);
    FGamePlatformResult StopEffect(
        const FGamePlatformCameraEffectHandle& Handle,
        bool bImmediately);

Mode Request 固定包含 DefinitionId、SourceId、Owner、TargetProviderObject；Look Input 固定包含 Value、Unit、DeltaSeconds、SourceId；Effect Request 固定包含 DefinitionId、RequestId、SourceId、Owner、Scale。查询只返回值语义 State／Snapshot／Diagnostics，不公开 Definition、Controller、CameraManager 或内部数组裸指针。

- [ ] **Step 1: Write contract and validation Automation tests**

新增测试：

    GamePlatform.Camera.Contracts.HandleShape
      valid only when ScopeId, HandleId, LocalPlayerGeneration and WorldGeneration are valid
      mode and effect handles are distinct types

    GamePlatform.Camera.Contracts.DefinitionIdentity
      mode PrimaryAssetId == GamePlatformDefinition:platform.camera.mode.exploration_default@1
      effect PrimaryAssetId == GamePlatformDefinition:platform.camera.effect.impact_default@1
      no custom Camera primary asset type is introduced

    GamePlatform.Camera.Contracts.DefinitionValidation
      FOV 0 and 180 fail
      negative blend duration fails
      non-finite offsets fail
      SweepFromPivot with non-positive radius fails
      target policy combinations follow bRequireTargetOnActivation and TargetLossPolicy

    GamePlatform.Camera.Contracts.ServiceBoundary
      public interface has the exact methods above
      target snapshot stores TWeakObjectPtr<UWorld>, not a retained raw world pointer

- [ ] **Step 2: Build Editor and verify RED**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -RunId ([guid]::NewGuid())

Expected: nonzero exit because the new test references missing Camera public contracts; record the fresh UBT log.

- [ ] **Step 3: Implement the public types and definitions**

Use these exact enum values:

    EGamePlatformCameraModeState:
      Invalid, PendingDefinition, Active, Suppressed,
      BlendingOut, Failed, Cancelled, Released

    EGamePlatformCameraLookUnit:
      DegreesDelta, DegreesPerSecond

    EGamePlatformCameraTargetPolicy:
      None, LookAt, LockOn

    EGamePlatformCameraTargetLossPolicy:
      FallbackUntargeted, ReleaseMode

    EGamePlatformCameraInputPolicy:
      Free, Disabled, TargetRelative

    EGamePlatformCameraCollisionPolicy:
      RespectViewTarget, Disabled, SweepFromPivot

    EGamePlatformCameraLagPolicy:
      RespectViewTarget, Disabled, Exponential

Definition 继承统一 Data 基类，不覆盖主资产类型；ValidateDefinition 先调用 Super，再执行 Camera 规则。Mode Definition 必须实现 ModeTag／Priority／DiagnosticName、FOV 策略与角度、Pivot／Camera／Distance Offset、Rotation Limits、BlendIn／BlendOut、Target／TargetLoss、Input、Collision／Lag 及 Sweep 参数；Effect Definition 必须实现 ShakeClass 软类引用、默认强度、播放空间、舒适度类别、去重和停止策略。Effect 的 ShakeClass 标记到 Camera Asset Bundle。

- [ ] **Step 4: Build and run contract Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera.Contracts" -RunId ([guid]::NewGuid())

Expected: Editor 构建 exit 0；报告只包含当前构建生成的 GamePlatform.Camera.Contracts.*，0 failed。

- [ ] **Step 5: Commit the public contract**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Definitions
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraContractTests.cpp
    git commit -m "feat: add GamePlatformCamera public contracts"

### Task 3：实现确定性模式栈与过渡

**Files:**

- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Types/GamePlatformCameraRuntimeTypes.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modes/GamePlatformCameraModeStack.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modes/GamePlatformCameraModeStack.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modes/GamePlatformCameraTransitionState.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modes/GamePlatformCameraTransitionState.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraModePolicyTests.cpp

**Interfaces:**

- Consumes: Task 2 handles、mode state、Definition copied runtime values。
- Produces:
  - FGamePlatformCameraModeStack::TryAddPending。
  - FGamePlatformCameraModeStack::Activate。
  - FGamePlatformCameraModeStack::Release。
  - FGamePlatformCameraModeStack::CompactExpiredOwners。
  - FGamePlatformCameraModeStack::GetEffective。
  - FGamePlatformCameraTransitionState::BeginToMode／BeginToBase／Retarget／Evaluate。

- [ ] **Step 1: Write failing policy tests**

新增测试：

    GamePlatform.Camera.Policy.Stack
      higher priority wins
      equal priority uses later ActivationSequence
      pending never wins
      releasing top restores lower entry
      duplicate Definition/Source/Owner still returns distinct handles
      entry 65 returns CameraCapacityExceeded and does not evict

    GamePlatform.Camera.Policy.AsyncActivationOrder
      request A accepted before B
      B completes first and receives activation sequence 1
      A completes later and receives sequence 2
      equal priority selects A because activation order, not request order

    GamePlatform.Camera.Policy.StaleAndTombstones
      wrong scope/world/generation cannot release
      repeated release succeeds while tombstone retained
      tombstone 129 evicts oldest and oldest returns CameraUnknownHandle

    GamePlatform.Camera.Policy.Transition
      acquire takeover uses incoming BlendIn
      release uses outgoing BlendOut
      interrupted blend starts from current emitted POV
      zero duration snaps without NaN
      world teardown clears without blend

- [ ] **Step 2: Build and verify RED**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -RunId ([guid]::NewGuid())

Expected: nonzero exit because ModeStack／TransitionState production files or methods are missing.

- [ ] **Step 3: Implement fixed-capacity policy**

Use TArray with Reserve(64) for active/pending records and a fixed FIFO of 128 tombstones. Sort or recompute only on mutation. ActivationSequence is int64 and is assigned only after successful Definition activation. Transition copies FMinimalViewInfo and all runtime settings by value; it never retains Definition pointers.

- [ ] **Step 4: Run policy Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera.Policy" -RunId ([guid]::NewGuid())

Expected: all four policy groups pass; no asset or world is required.

- [ ] **Step 5: Commit the policy**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Types
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modes
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraModePolicyTests.cpp
    git commit -m "feat: add deterministic camera mode policy"

### Task 4：实现 LocalPlayer 服务与数据生命周期

**Files:**

- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Subsystems/GamePlatformCameraLocalPlayerSubsystem.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Subsystems/GamePlatformCameraLocalPlayerSubsystem.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Loading/GamePlatformCameraDefinitionLoader.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Loading/GamePlatformCameraDefinitionLoader.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Loading/GamePlatformCameraDefinitionCoordinator.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Loading/GamePlatformCameraDefinitionCoordinator.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Diagnostics/GamePlatformCameraDiagnostics.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Diagnostics/GamePlatformCameraDiagnostics.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraServiceTests.cpp

**Interfaces:**

- Consumes: Task 2 service contract、Task 3 stack、IGamePlatformDataService::AcquireDefinition／GetLoadedDefinition／ReleaseDefinition。
- Produces:
  - UGamePlatformCameraLocalPlayerSubsystem 实现 IGamePlatformCameraService。
  - 私有 IGamePlatformCameraDefinitionLoader 测试接缝与生产 FGamePlatformCameraDataDefinitionLoader。
  - Mode Acquire／Release、状态查询、世界代次、Owner 清理和数据租约终态。
  - WITH_DEV_AUTOMATION_TESTS 下的 SetDefinitionLoaderForTesting；Shipping 不导出测试入口。

- [ ] **Step 1: Write failing service lifecycle tests**

新增测试：

    GamePlatform.Camera.Service.AcquireRelease
      accepted request returns PendingDefinition handle
      successful fake load copies Definition runtime values then becomes Active/Suppressed
      release cancels pending or releases active lease exactly once

    GamePlatform.Camera.Service.StaleCompletion
      world generation changes while fake load pending
      late success callback returns Cancelled
      stack remains empty and lease is released

    GamePlatform.Camera.Service.ScopeIsolation
      two LocalPlayer service instances reject each other's handles
      old world handle cannot release new world mode

    GamePlatform.Camera.Service.OwnerLifecycle
      expired pending owner never activates
      effective expired owner is released
      suppressed expired owner is compacted after GC/mutation

    GamePlatform.Camera.Service.SubsystemAndWorldLifecycle
      LocalPlayer subsystem initialize/deinitialize installs and removes only owned delegates
      two LocalPlayers remain isolated
      world travel increments generation, cancels pending, releases active leases and clears old bindings
      late callbacks after deinitialize are terminal and side-effect free

    GamePlatform.Camera.Service.FailureRollback
      missing service, invalid ID, wrong class, failed load and capacity rejection
      never leave half-active records or retained leases
      an already-active lower mode remains effective when a new Definition fails

- [ ] **Step 2: Build and verify RED**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -RunId ([guid]::NewGuid())

Expected: nonzero exit because subsystem and coordinator are missing.

- [ ] **Step 3: Implement service and deferred completion**

IGamePlatformDataService calls use:

    ExpectedClass = UGamePlatformCameraModeDefinition::StaticClass()
        or UGamePlatformCameraEffectDefinition::StaticClass()
    Bundles = ["Core", "Camera"]
    Lifetime = EGamePlatformDataLifetime::World
    WeakCaller = current LocalPlayer subsystem

Completion 必须重新校验 ScopeId、LocalPlayerGeneration、WorldGeneration、HandleId、Owner 和取消状态后才激活。Initialize 只建立身份、固定容量和委托；Deinitialize 严格按规格逆序清理。

- [ ] **Step 4: Run service Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera.Service" -RunId ([guid]::NewGuid())

Expected: service tests pass；诊断计数匹配测试中的 accepted、rejected、cancelled、failed、released 数量。

- [ ] **Step 5: Commit the service**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Subsystems
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Subsystems
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Loading
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Diagnostics
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraServiceTests.cpp
    git commit -m "feat: add local player camera service"

### Task 5：接入 PlayerCameraManager、Modifier、目标、碰撞与 Look

**Files:**

- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Bindings/GamePlatformCameraPlayerBinding.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Bindings/GamePlatformCameraPlayerBinding.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Execution/GamePlatformCameraEvaluator.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Execution/GamePlatformCameraEvaluator.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Execution/GamePlatformCameraCollisionQuery.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Execution/GamePlatformCameraCollisionQuery.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modifiers/GamePlatformCameraModifier.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modifiers/GamePlatformCameraModifier.cpp
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Subsystems/GamePlatformCameraLocalPlayerSubsystem.h
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Subsystems/GamePlatformCameraLocalPlayerSubsystem.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraEngineTests.cpp

**Interfaces:**

- Consumes: Task 3 effective runtime view／transition；Task 4 service state；APlayerCameraManager APIs verified in UE5.8。
- Produces:
  - FGamePlatformCameraPlayerBinding::Refresh(APlayerController*)／Reset。
  - FGamePlatformCameraEvaluator::Evaluate。
  - IGamePlatformCameraCollisionQuery private seam and World implementation。
  - UGamePlatformCameraModifier::ModifyCamera／ProcessViewRotation。
  - SubmitLookInput final consumption。

- [ ] **Step 1: Write failing engine behavior tests**

新增测试：

    GamePlatform.Camera.Engine.BasePassthrough
      no active mode leaves location, rotation and FOV unchanged

    GamePlatform.Camera.Engine.ModeEvaluation
      FOV override, local pivot, camera offset, rotation limits and blend are deterministic

    GamePlatform.Camera.Engine.LookUnits
      DegreesDelta applies Value once
      DegreesPerSecond applies Value * DeltaSeconds once
      negative/non-finite DeltaSeconds or values are rejected
      Disabled input policy does not call AddYawInput/AddPitchInput

    GamePlatform.Camera.Engine.InvalidTargetSnapshot
      wrong world and NaN target never modify POV
      bRequireTargetOnActivation rejects
      runtime loss follows FallbackUntargeted or ReleaseMode

    GamePlatform.Camera.Engine.Collision
      RespectViewTarget makes zero plugin traces
      Disabled makes zero traces
      SweepFromPivot makes at most one sphere sweep and clamps to safe point

    GamePlatform.Camera.Engine.ManagerReplacement
      same controller receives a replacement CameraManager
      next Look/mode/effect request removes old binding and installs one modifier on new manager

    GamePlatform.Camera.Engine.ControllerPawnAndModifierLifecycle
      PlayerControllerChanged removes the old modifier before installing exactly one on the new manager
      Pawn respawn refreshes the base POV without duplicating the modifier
      deinitialize and world cleanup remove only the modifier owned by this LocalPlayer service
      RespectViewTarget preserves the base POV already produced by the view target or SpringArm

- [ ] **Step 2: Build and verify RED**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -RunId ([guid]::NewGuid())

Expected: nonzero exit because binding、evaluator and modifier are missing.

- [ ] **Step 3: Implement UE native adapter**

PlayerControllerChanged 调用 RefreshBinding。Modifier 每帧只读取发布的值快照；当前有效 Owner 每帧一次弱检查；碰撞接口只在 SweepFromPivot 路径调用一次。SubmitLookInput 先 RefreshBinding，再按模式输入策略调用当前 Controller 的 AddYawInput／AddPitchInput。

声明 CSV／STAT：

    GamePlatformCamera.EvaluateCamera
    GamePlatformCamera.AdvanceTransition
    GamePlatformCamera.CollisionQuery
    GamePlatformCamera.ActiveModeCount
    GamePlatformCamera.PendingDefinitionCount
    GamePlatformCamera.ActiveEffectCount
    GamePlatformCamera.TargetLostCount
    GamePlatformCamera.CapacityRejectedCount

- [ ] **Step 4: Run engine Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera.Engine" -RunId ([guid]::NewGuid())

Expected: all engine tests pass；测试日志中没有 Accessed None、ensure、NaN 或旧 Manager 残留。

- [ ] **Step 5: Commit the engine adapter**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Bindings
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Execution
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Modifiers
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Public/Subsystems/GamePlatformCameraLocalPlayerSubsystem.h
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Subsystems/GamePlatformCameraLocalPlayerSubsystem.cpp
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraEngineTests.cpp
    git commit -m "feat: integrate Unreal camera execution"

### Task 6：增加瞬时效果与 Presentation Provider

**Files:**

- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Effects/GamePlatformCameraEffectController.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Effects/GamePlatformCameraEffectController.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Presentation/GamePlatformCameraPresentationProvider.h
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Presentation/GamePlatformCameraPresentationProvider.cpp
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Subsystems/GamePlatformCameraLocalPlayerSubsystem.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraEffectTests.cpp
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraPresentationTests.cpp

**Interfaces:**

- Consumes: Task 4 effect Definition loading；Task 5 current PlayerCameraManager binding；UGamePlatformPresentationClientSubsystem。
- Produces:
  - RequestEffect／StopEffect 生产实现。
  - FGamePlatformCameraPresentationProvider::Handle。
  - ProviderId Camera、ProviderChannel Camera、Priority 100。
  - RequestId 有界去重和 prediction correction。

- [ ] **Step 1: Write failing effect tests**

新增测试：

    GamePlatform.Camera.Effects.Lifecycle
      pending effect can cancel
      successful effect calls StartCameraShake once
      StopEffect stops only the owned weak shake instance
      manager/world teardown stops and releases all owned effects
      manager replacement stops old-manager owned instances and the next request uses the new manager
      effect 65 returns CameraCapacityExceeded

    GamePlatform.Camera.Effects.Scale
      negative or non-finite scale fails
      effective scale combines request and Definition without exceeding validated comfort clamp

- [ ] **Step 2: Write failing Presentation tests**

新增测试：

    GamePlatform.Camera.Presentation.Channel
      non-Camera channel returns false
      Instant/Timed valid effect request returns true
      Persistent returns false
      stale WorldGeneration returns false

    GamePlatform.Camera.Presentation.Prediction
      Predicted plays once
      matching Confirmed does not replay
      Corrected replaces according to effect policy
      Cancelled stops owned effect

    GamePlatform.Camera.Presentation.InvalidAndEvictedRequests
      malformed or non-effect GamePlatformDefinition ID returns false
      duplicate Confirmed does not replay
      after 128 tombstones oldest Cancelled returns unknown and does not affect live effects

- [ ] **Step 3: Build and verify RED**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -RunId ([guid]::NewGuid())

Expected: nonzero exit because effect controller and provider are missing.

- [ ] **Step 4: Implement effects and register Provider**

Camera subsystem Initialize 取得同 LocalPlayer 的 Presentation 子系统并注册 Provider；Deinitialize 第一阶段注销。Catalog DefinitionId 严格解析为 GamePlatformDefinition:<platform.camera.effect.*@version>，AcquireDefinition 使用 UGamePlatformCameraEffectDefinition ExpectedClass。不得按文件路径加载。Presentation RequestId／SourceId 原样传入效果请求，Owner 绑定当前 Provider 生命周期；Magnitude 必须有限且非负并作为 Scale，零值明确表示关闭该可选效果，不把零偷偷改写为一。

- [ ] **Step 5: Run effects and Presentation Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera.Effects" -RunId ([guid]::NewGuid())
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera.Presentation" -RunId ([guid]::NewGuid())

Expected: both runs exit 0；每份报告只接受当前 RunId。

- [ ] **Step 6: Commit effects and Presentation**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Effects
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Presentation
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Subsystems/GamePlatformCameraLocalPlayerSubsystem.cpp
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraEffectTests.cpp
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/Private/Tests/GamePlatformCameraPresentationTests.cpp
    git commit -m "feat: add camera effects and presentation provider"

### Task 7：接入 DBAClient 输入与既有 InWorld 事实

**Files:**

- Modify: Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient/DivineBeastsInputClient.Build.cs
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient/Private/Subsystems/DivineBeastsInputClientSubsystem.cpp
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient/Private/Tests/DivineBeastsInputTests.cpp
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Public/Tags/DivineBeastsPresentationTags.h
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Tags/DivineBeastsPresentationTags.cpp
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Public/Catalog/DivineBeastsPresentationProjectCatalog.h
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Catalog/DivineBeastsPresentationProjectCatalog.cpp
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Tests/DivineBeastsPresentationRuntimeTests.cpp
- Modify: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/DivineBeastsPresentationClient.Build.cs
- Create: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Camera/DivineBeastsCameraCompositionSettings.h
- Create: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Camera/DivineBeastsCameraCompositionSettings.cpp
- Create: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Camera/DivineBeastsCameraCompositionPolicy.h
- Create: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Camera/DivineBeastsCameraClientSubsystem.h
- Create: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Camera/DivineBeastsCameraClientSubsystem.cpp
- Create: Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Tests/DivineBeastsCameraCompositionTests.cpp
- Modify: Tests/Architecture/PluginCompositionAudit.Tests.ps1

**Interfaces:**

- Consumes: IGamePlatformCameraService、FDivineBeastsFlowViewStateChangedNative、FDivineBeastsFlowNodes::InWorld。
- Produces:
  - Input Look 的唯一 Camera 消费路径。
  - 私有 UDivineBeastsCameraClientSubsystem。
  - UDivineBeastsCameraCompositionSettings::DefaultWorldCameraModeId，默认 GamePlatformDefinition:platform.camera.mode.exploration_default@1。
  - InWorld 进入申请、离开释放、世界清理释放。
  - 复用现有 FDivineBeastsPresentationProjectCatalog 的项目级 Camera Impact 条目；现有 DivineBeastsPresentationClientSubsystem 继续负责一次注册和逆序注销，不创建第二个 Catalog 注册器。

- [ ] **Step 1: Extend failing architecture and input tests**

断言：

    DBAClient plugin depends on GamePlatformCamera
    Client and Editor composition reach GamePlatformCameraClient
    Server composition does not reach it and Server Target disables plugin
    HandleMoveLookInput calls SubmitLookInput
    HandleMoveLookInput contains no AddYawInput/AddPitchInput and no DeltaSeconds multiplication
    TargetLock path still only broadcasts project request; it does not acquire Camera lock mode
    project catalog revision is incremented and contains exactly one Camera entry:
      SemanticTag = DivineBeasts.Presentation.Camera.Impact
      ProviderChannel = Camera
      DefinitionId = GamePlatformDefinition:platform.camera.effect.impact_default@1

- [ ] **Step 2: Write failing composition tests**

新增：

    DivineBeasts.Camera.Composition.WorldLifecycle
      non-InWorld state owns no mode
      entering InWorld acquires exactly one default mode
      repeated same FlowRunId/NodeGeneration is idempotent
      leaving InWorld releases it
      new world generation never reuses old handle

    DivineBeasts.Camera.Composition.MissingCamera
      unavailable Camera service reports local diagnostic and does not create fallback AddYaw path

    DivineBeasts.Camera.Composition.ProjectCatalog
      existing default fragment remains valid and deterministic
      Camera Impact resolves to the Camera provider and approved effect Definition
      existing VFX entries remain unchanged

- [ ] **Step 3: Run tests and verify RED**

Run:

    Invoke-Pester -Path ./Tests/Architecture/PluginCompositionAudit.Tests.ps1 -Output Detailed
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -RunId ([guid]::NewGuid())

Expected: Pester or Editor build fails because DBAClient has not declared／implemented Camera integration.

- [ ] **Step 4: Migrate Look and add project composition**

DivineBeastsInputClient 只把 Event.Value、单位和当前 DeltaSeconds 传给 SubmitLookInput；Camera 负责速率积分。DivineBeastsCameraClientSubsystem 从 GameInstance 获取 ApplicationFlow，订阅 OnViewStateChanged；只在 InWorld 使用 Settings 中的默认 Definition Id。项目 Camera Impact 语义只扩展现有默认 Catalog Fragment 并递增 CatalogRevision；不改动既有 VFX 条目、不重复注册 Fragment。没有正式目标／死亡／观战事实时不得添加模拟生产回调。

- [ ] **Step 5: Run DBA and architecture tests**

Run:

    Invoke-Pester -Path ./Tests/Architecture/PluginCompositionAudit.Tests.ps1 -Output Detailed
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "DivineBeasts.Input" -RunId ([guid]::NewGuid())
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "DivineBeasts.Camera" -RunId ([guid]::NewGuid())

Expected: all pass；Server closure excludes Camera；Look has one final consumer。

- [ ] **Step 6: Commit project integration**

    git add Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin
    git add Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient
    git add Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime
    git add Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient
    git add Tests/Architecture/PluginCompositionAudit.Tests.ps1
    git commit -m "feat: integrate project camera composition"

### Task 8：使用 UE 生成真实 Camera Definition 与 Review Map

**Files:**

- Create: Tools/AssetTools/CreateGamePlatformCameraReviewAssets.py
- Create: Tests/Camera/Assets/test_create_gameplatform_camera_review_assets.py
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Definitions/DA_GPCamera_Mode_Exploration_Default.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Definitions/DA_GPCamera_Mode_Combat_Default.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Definitions/DA_GPCamera_Mode_LockOn_Default.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Definitions/DA_GPCamera_Mode_Spectator_Default.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Definitions/DA_GPCamera_Mode_Death_Default.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Definitions/DA_GPCamera_Effect_Impact_Default.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Review/Effects/BP_GPCamera_Shake_Impact.uasset
- Generate with UE: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content/Review/Maps/L_GPCamera_Review.umap
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ReviewAssetManifest.json
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ManualReview.md

**Interfaces:**

- Consumes: Task 2 Definition classes、Task 5/6 runtime、UE Editor Python API。
- Produces: 幂等且带生成所有权元数据的平台默认资产、真实 Review Map 和待人工审核清单。

- [ ] **Step 1: Write Python RED tests**

测试函数：

    parse_args(argv)
    validate_unreal_environment(unreal_module)
    create_definition_assets(unreal_module, overwrite_owned=False)
    create_review_map(unreal_module, overwrite_owned=False)
    write_manifest(path, records)

断言：

    importing without unreal does not create files
    dry validation never writes .uasset/.umap text
    existing asset without GamePlatformGeneratedBy metadata is never overwritten
    rerun updates only assets owned by this generator
    logical IDs use GamePlatformDefinition and approved namespaces
    map contains scenarios for exploration, combat, lock-on, spectator,
        death, collision, high-speed turn, modal input, two local-player markers,
        zero/reduced/full Shake Scale, reduced-motion comparison,
        multi-resolution/safe-zone notes and post-travel stale-handle verification

- [ ] **Step 2: Run Python tests and verify RED**

Run:

    python ./Tests/Camera/Assets/test_create_gameplatform_camera_review_assets.py -v

Expected: failures because generator functions do not exist.

- [ ] **Step 3: Implement the UE-only generator**

Follow CreateFoundationAssets.py process safety: full UnrealEditor.exe with ScriptErrorsAreFatal，no filesystem fake assets，no deletion of user assets。每个 Definition 写入 LogicalId、DataVersion、模式字段并保存；Shake 必须是实际可加载的 UCameraShakeBase 派生资产；Review Map 必须有真实几何、碰撞障碍和可执行 Camera 场景说明。

- [ ] **Step 4: Run Python tests GREEN**

Run:

    python ./Tests/Camera/Assets/test_create_gameplatform_camera_review_assets.py -v

Expected: all tests pass without creating binary assets outside UE.

- [ ] **Step 5: Run the locked UE5.8 editor generator**

Run:

    $editor = Join-Path $env:UE_ROOT "Engine/Binaries/Win64/UnrealEditor.exe"
    $project = Resolve-Path "./Game/DivineBeastsArena.uproject"
    $script = Resolve-Path "./Tools/AssetTools/CreateGamePlatformCameraReviewAssets.py"
    & $editor $project -unattended -nop4 -nosplash -ScriptErrorsAreFatal "-ExecutePythonScript=$script"

Expected: exit 0；所有列出的资产由 UE 保存；重开编辑器后 AssetRegistry 能以 GamePlatformDefinition:<logical-id> 找到 Definition，Review Map 可加载。

- [ ] **Step 6: Run real-data Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera" -RunId ([guid]::NewGuid())

Expected: Data 成功加载模式和效果 Definition；不存在类型不匹配或硬路径加载。

- [ ] **Step 7: Commit generated assets and generator**

    git add Tools/AssetTools/CreateGamePlatformCameraReviewAssets.py
    git add Tests/Camera/Assets/test_create_gameplatform_camera_review_assets.py
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Content
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ReviewAssetManifest.json
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ManualReview.md
    git commit -m "assets: add GamePlatformCamera review content"

### Task 9：补齐 Cook、服务器审计、API 文档与交付证据

**Files:**

- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/CookGamePlatformCameraClient.ps1
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/AuditGamePlatformCameraServerStage.ps1
- Modify: Build/Validation/VerifyCamera.ps1
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/Architecture.md
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/API.md
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/TestingAndEvidence.md
- Create: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/PerformanceAndSecurity.md
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/README.md
- Modify: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ImplementationSpecification.md
- Modify: Docs/Architecture/游戏端插件清单设计.md
- Modify: Docs/Architecture/游戏端插件系统P0收敛审计.md
- Modify: Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md
- Modify: Docs/CHANGELOG.md

**Interfaces:**

- Consumes: Tasks 1–8 complete source、tests、assets、existing FoundationTools。
- Produces: 一条不会伪报的综合验证入口、Client Review Cook、Server Stage 漏出审计、准确的实现／未执行文档。

- [ ] **Step 1: Write failing script contract tests**

在 VerifyCamera.ps1 的静态自检中要求：

    each selected operation writes a unique Saved/Validation/Camera/<RunId> result
    missing UE_ROOT returns NotExecuted/2, not Passed
    external exit code is preserved
    Automation requires same-run Editor build
    Client Shipping has its own UBT invocation and cannot inherit Development status
    Server audit inspects build receipt and loose staged files
    packed containers without a supported listing path remain Incomplete, not Passed
    scripts terminate only their own PID

- [ ] **Step 2: Run validation scripts and verify RED**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -Static -RunId ([guid]::NewGuid())

Expected: nonzero until Cook／audit and required documentation paths exist.

- [ ] **Step 3: Implement Cook and Server audit**

Client Cook uses target DivineBeastsArenaClient and map /GamePlatformCamera/Review/Maps/L_GPCamera_Review。Server audit rejects GamePlatformCameraClient、GamePlatformCameraEditor、L_GPCamera_Review、DA_GPCamera_、BP_GPCamera_Shake_ from receipts and stage. It must report packed-container coverage separately.

- [ ] **Step 4: Generate documentation from final source**

API.md records exact signatures, arguments, return, failure, cancellation, thread and lifecycle semantics。Architecture.md records ownership and data flow。PerformanceAndSecurity.md records measured counters and no invented budgets，并明确长期 Motion Reduction／Shake Scale／FOV Comfort 持久化在 GamePlatformSettings 单一真源迁移前仍未实现。TestingAndEvidence.md records every command、RunId、exit code and distinguishes Passed／Failed／NotExecuted／PendingHumanReview。

README and global documents must state actual implementation, not planned claims. Manual Review remains Pending until a human fills Reviewer and explicit confirmation.

- [ ] **Step 5: Run static and documentation checks GREEN**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -Static -RunId ([guid]::NewGuid())
    & ./Tests/Architecture/ValidateDesignBaseline.ps1
    git diff --check

Expected: exit 0 for static checks；output explicitly says no UE/Cook/Review inference。

- [ ] **Step 6: Commit delivery tooling and docs**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests
    git add Build/Validation/VerifyCamera.ps1
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/README.md
    git add Docs/Architecture/游戏端插件清单设计.md
    git add Docs/Architecture/游戏端插件系统P0收敛审计.md
    git add Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md
    git add Docs/CHANGELOG.md
    git commit -m "docs: add GamePlatformCamera delivery evidence"

### Task 10：执行最终构建、Automation、Cook、产物审计与评审交接

**Files:**

- Modify only if a fresh failure is reproduced: the owning production/test file from Tasks 1–9
- Update after commands: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/TestingAndEvidence.md
- Update after commands: Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ManualReview.md

**Interfaces:**

- Consumes: complete plan deliverables。
- Produces: fresh machine-readable verification evidence and an honest final status；does not auto-sign human review。

- [ ] **Step 1: Run all static and Python tests**

Run:

    & ./Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Tests/Scripts/TestGamePlatformCameraArchitecture.ps1
    Invoke-Pester -Path ./Tests/Architecture/PluginModuleGraph.Tests.ps1 -Output Detailed
    Invoke-Pester -Path ./Tests/Architecture/PluginCompositionAudit.Tests.ps1 -Output Detailed
    & ./Tests/Architecture/ValidateDesignBaseline.ps1
    python ./Tests/Camera/Assets/test_create_gameplatform_camera_review_assets.py -v

Expected: all exit 0；no skipped required assertion。

- [ ] **Step 2: Build Editor, Client Development／Shipping, and Server sequentially**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -BuildClient -BuildServer -RunId ([guid]::NewGuid())
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildClientShipping -RunId ([guid]::NewGuid())

Expected: Editor／Client Development／Server Development 在第一份 RunId 中各自 exit 0，Client Shipping 在独立 RunId 中 exit 0；Server receipt contains no Camera client module。不存在的普通 Game Target 记录为 NotApplicable 及实际路径证据，不得标记 Passed。

- [ ] **Step 3: Run full Camera Automation**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "GamePlatform.Camera" -RunId ([guid]::NewGuid())
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "DivineBeasts.Camera" -RunId ([guid]::NewGuid())
    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -BuildEditor -Automation -AutomationFilter "DivineBeasts.Input" -RunId ([guid]::NewGuid())

Expected: report export exists for each fresh run；0 failed；no stale binary inference。

- [ ] **Step 4: Cook Client Review and production Server stage**

Run:

    & ./Build/Validation/VerifyCamera.ps1 -EngineRoot $env:UE_ROOT -CookClient -CookServer -RunId ([guid]::NewGuid())

Expected: Client stage contains loadable Camera definitions and Review Map；Server stage audit finds zero Camera modules/assets。If packed contents cannot be enumerated, result is Incomplete/2 rather than Passed。

- [ ] **Step 5: Capture performance and manual review as separate states**

Open L_GPCamera_Review in UE Editor and execute the ManualReview.md matrix for exploration、combat、lock-on、arena-style intro、spectator、death、collision、high-speed turn、modal input、mouse/gamepad、30/60/120 FPS、multi-resolution/safe-zone、split-screen、travel、zero/reduced/full Shake Scale and reduced-motion comparison。Automation may populate metrics and Pending report；only the human reviewer may set Passed。

Expected: Unreal Insights／CSV records the approved counters with platform、build、map、resolution and sample duration；no fabricated threshold。

- [ ] **Step 6: Update evidence and run final diff audit**

Run:

    git status --short
    git diff --check
    git diff --stat
    rg -n "TODO|TBD|FIXME|伪造|未执行|PendingHumanReview" Game/Plugins/GamePlatform/Presentation/GamePlatformCamera

Expected: only intentional source/docs/assets are present；no Saved、Intermediate、Binaries、logs、credentials or text .uasset/.umap。

- [ ] **Step 7: Commit the fresh evidence status**

    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/TestingAndEvidence.md
    git add Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Docs/ManualReview.md
    git commit -m "test: record GamePlatformCamera verification status"

- [ ] **Step 8: Request final independent review**

Reviewer checks:

    spec coverage and plan checkboxes
    platform/MOBA/project dependency direction
    game-thread and world-generation safety
    no double Look integration
    no server Camera artifacts
    no false UE/Cook/Review claims
    complete Chinese comments for every changed first-party file

Only after all required machine gates pass and human Review remains accurately labeled may the implementation be handed off as complete.
