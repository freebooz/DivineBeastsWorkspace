# GamePlatformInput（游戏平台输入）测试与验证证据

> 更新日期：2026-09-27。

## 1. 审查前真实状态

审查确认 Public（公开）接口、Profile、Semantic（语义）和测试文件已经存在，但 HEAD 中从未存在：

- `Private/Policy/InputPolicy.h`。
- `Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp`。

因此审查前属于“契约基础已写、核心执行层缺失”，不能按插件清单旧描述理解为完整运行实现。

直接尝试 `DivineBeastsArenaClient -Module=GamePlatformInputClient` 时，因为插件没有进入当前目标启用闭包，UBT 返回 `Unable to find output items for module`；这不是源码编译通过。

## 2. Native（原生）策略测试

入口：`Tests/CMakeLists.txt`，直接编译生产 `InputPolicy.h`。

工具链：

```text
CMake 3.31.6-msvc6
Visual Studio 2022 BuildTools
MSVC 19.44.35228
Windows SDK 10.0.26100.0
C++17
/W4 /WX /permissive- /fp:precise
```

结果：

```text
Debug   1/1 Passed
Release 1/1 Passed
410 assertions, 0 failures
```

覆盖：

- Context Lease（上下文租约）所有权、共享和容量。
- Block Lease（阻断租约）叠加和乱序释放。
- Action Gate（动作门禁）中断、重新武装、非法值。
- 安全二维轴归一化和超大/非有限数。
- PC手柄/移动虚拟摇杆径向死区。
- 视角灵敏度与水平/垂直反转。
- Touch移动幅度倍率放大后的单位圆约束。
- Touch独立视角灵敏度与通用灵敏度组合。
- BlockLedger缓存组合掩码：重叠租约、乱序释放和最后引用清零，证明高频阻断查询可以使用 O(1) 缓存结果。
- SemanticId/Descriptor → InputProfileCompiler → CompactSlot 的可扩展语义编译路径。
- `EGamePlatformBuiltInInputSemantic` 的7个跨游戏公共语义与项目 `DivineBeasts.Input.*` 分层合同。
- 本地设置稳定键隔离。

## 3. UE5.8 模块构建

### 3.1 可扩展语义迁移与项目输入闭包验证

本轮在正式工程中完成 `SemanticId/Descriptor → InputProfileCompiler → CompactSlot → DivineBeastsInputClient → GamePlatformAbilitySystem` 定向构建。平台新增 `EGamePlatformBuiltInInputSemantic` 后，`GamePlatformInputClient` Editor/Win64 Client 均成功；`DivineBeastsInputClient` Editor/Win64 Client 均成功。

联调中发现既有 `UGamePlatformAbilitySetDefinition::ValidateDefinition()` 只有声明、没有实现，导致 `GamePlatformAbilitySystem` 链接失败；补充纯字段校验实现并用 `-NoUBTMakefiles` 强制重采集新源文件后，AbilitySystem Editor/Win64 Client 也成功。该修复不加载软资源、不改变GAS业务行为，只补齐已公开声明的Definition校验合同。

```text
DivineBeastsArenaEditor -Module=GamePlatformInputClient       → Succeeded
DivineBeastsArenaEditor -Module=DivineBeastsInputClient      → Succeeded
DivineBeastsArenaEditor -Module=GamePlatformAbilitySystem     → Succeeded
DivineBeastsArenaClient -Module=GamePlatformInputClient       → Succeeded
DivineBeastsArenaClient -Module=DivineBeastsInputClient      → Succeeded
DivineBeastsArenaClient -Module=GamePlatformAbilitySystem     → Succeeded
```

项目 `TestInputArchitecture.ps1（输入架构门禁）` 通过，确认项目运行时桥不复制平台Input子系统、不引入业务Tick，并禁止重新使用平台旧 Attack/AbilitySlot/TargetLock 兼容语义。

本轮已使用：

```text
D:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat
-EnablePlugin=GamePlatformInput
-Module=GamePlatformInputClient
```

实际结果：

```text
DivineBeastsArenaEditor Win64 Development -Module=GamePlatformInputClient  → Succeeded
DivineBeastsArenaClient Win64 Development -Module=GamePlatformInputClient  → Succeeded
```

Android 也已实际尝试：

```text
DivineBeastsArenaClient Android Development -Module=GamePlatformInputClient
→ Failed before C++ compile
→ SDK validation failed: Android SDK/NDK not found, required NDK r27c
→ Exit 6
```

因此 Windows PC 代码端 G1 编译已取得证据；Android 只能证明构建入口已检查到移动目标，不能证明移动 C++ 编译通过。iOS 在本 Windows Runner 上未执行，需要 macOS/Xcode/远程构建链。
本轮第二次优化后再次执行 Editor/Win64 Client `-Module=GamePlatformInputClient`，两者仍为 `Result: Succeeded`，覆盖 O(1) BlockLedger、固定 ActionGate 数组、目标平台默认设备族和 DeviceRevision 新实现。

随后实际启动 `UnrealEditor-Cmd.exe` 两次尝试 `Automation RunTests GamePlatform.Input`。测试队列尚未开始，编辑器启动阶段强制执行 `ValidatePlatforms -AllPlatforms`：Win64 有效，但 Android 缺 r27c，VisionOS SDK 描述缺 `MainVersion`，进程退出1/3。该结果属于环境/引擎 SDK 校验阻断，不是 `GamePlatform.Input.*` 自动化用例失败；不得写成 UE Automation 已执行通过。

## 4. UE Automation（虚幻自动化）

当前新增执行层尚未建立真实 Input Profile `.uasset`，也没有可启动的完整游戏输入场景，因此不宣称 UE Automation/真实设备通过。

后续必须至少覆盖：

- 两个 LocalPlayer 隔离。
- Context 租约优先级。
- Block/UI仲裁。
- Controller/Pawn 切换解绑。
- Profile Data Lease 生命周期。
- 玩家重绑定与重启恢复。
- Touch Pointer 生命周期。
- 设备族变化。
- 焦点丢失/恢复。

## 5. PC与移动端真机证据

尚未执行：

- Windows 键鼠实际输入。
- Xbox/兼容手柄切换与重绑定。
- Android Touch/虚拟摇杆。
- iOS Touch/虚拟摇杆。
- 多点触控冲突。
- 移动端帧率/输入延迟。

这些不能由 C++ 编译或原生策略测试替代。

## 6. 当前验收状态

```text
G0 Architecture      Input范围Passed：三层继承/Public API边界与项目输入架构门禁通过；全局设计基线另有DBAArena→GamePlatformUIClient依赖声明问题，与Input无关
G1 Compile           PC Passed：GamePlatformInputClient、DivineBeastsInputClient、GamePlatformAbilitySystem 的Editor/Win64 Client定向构建通过；Android缺NDK r27c；iOS未执行
G2 Functional        Partial：Native 410断言通过；UE Automation被全平台SDK校验阻断，未进入测试队列
G3 Integration       Partial：项目Semantic/Profile/Move-Look/GAS/TargetLock桥已编码并通过正式模块编译；无真实Input资产及运行场景验收
G4 Manual Review     Not Passed
G5 Production        Not Passed：无PC/移动真机、Cook/Stage和性能数据
```