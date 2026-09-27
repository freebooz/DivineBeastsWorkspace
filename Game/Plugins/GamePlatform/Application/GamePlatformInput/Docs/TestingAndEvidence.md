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
401 assertions, 0 failures
```

覆盖：

- Context Lease（上下文租约）所有权、共享和容量。
- Block Lease（阻断租约）叠加和乱序释放。
- Action Gate（动作门禁）中断、重新武装、非法值。
- 安全二维轴归一化和超大/非有限数。
- PC手柄/移动虚拟摇杆径向死区。
- 视角灵敏度与水平/垂直反转。
- 本地设置稳定键隔离。

## 3. UE5.8 模块构建

本轮将在共享 UE 构建锁释放后使用：

```text
D:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat
-EnablePlugin=GamePlatformInput
-Module=GamePlatformInputClient
```

分别验证 Editor（编辑器）和 Client（客户端）。

当前状态：**待本轮最终编译结果回写**。

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
G0 Architecture      本轮设计已补齐，待最终全局回归
G1 Compile           待UE5.8最终构建结果
G2 Functional        Partial：Native 401断言通过
G3 Integration       Not Passed：无真实Profile/项目消费链运行
G4 Manual Review     Not Passed
G5 Production        Not Passed：无PC/移动真机、Cook/Stage和性能数据
```