# GamePlatformInput（游戏平台输入）API说明

> 所有接口只允许游戏线程调用。服务属于显式 `ULocalPlayer（本地玩家）`，不存在 Player0 全局替身。

## 1. 服务入口

```cpp
IGamePlatformInputService* Input =
    IGamePlatformInputService::Get(LocalPlayer);
```

## 2. Profile（配置）

`PrepareInputProfile(ProfileId, LocalSettingsKey, OutResult)` 使用 `GamePlatformData` 的 `Input` Bundle 异步准备配置。同步 Success（成功）只代表请求被接纳；最终 `bProfilePrepared` 从 Snapshot（快照）读取。

`ReleaseInputProfile` 精确释放本配置持有的 Data Lease、上下文、绑定和 Touch 来源。

## 3. Mapping Context（映射上下文）

```cpp
AcquireInputContext(ContextName, Priority, Owner, OutResult)
ReleaseInputContext(Handle)
```

- Priority 范围0..100。
- 多拥有者共享时使用当前租约最大优先级。
- Profile 标记不可共享的 Context 第二个拥有者会被拒绝。
- 如果相同 IMC 已被插件外部加入，平台拒绝接管。

## 4. Receiver（输入接收器）

`BindInputReceiver(UEnhancedInputComponent&)` 只接受当前本地控制器或其 Pawn 的组件。

每个 Action 绑定 Started/Ongoing/Triggered/Completed/Canceled 五个阶段；解绑只移除本插件保存的原生绑定句柄，不清除其他系统绑定。

## 5. Block（阻断）

```cpp
AcquireInputBlock(Channels, Reason, Owner, OutResult)
ReleaseInputBlock(Handle)
```

多个来源位掩码叠加，任何重叠位都会阻断对应 Semantic（语义）。

## 6. 设备族

```cpp
NotifyInputDeviceActivity(
    EGamePlatformInputDeviceFamily::KeyboardMouse);
```

设备族包括 Unknown / KeyboardMouse / Gamepad / Touch（未知/键鼠/手柄/触控）。Touch 入口会自动切换为 Touch；PC 手柄与键鼠切换由真实平台设备检测桥上报。
默认设备族按编译目标决定：Android/iOS 为 Touch，桌面为 KeyboardMouse；触屏 PC 不会仅因硬件支持 Touch 就误用移动提示。`FGamePlatformInputSnapshot::DeviceRevision` 只在设备族真实变化时递增，UI 可据此避免重复刷新提示资源。

## 7. 无障碍/舒适度

```cpp
SetAccessibilitySettings(Settings);
GetAccessibilitySettings();
```

当前支持通用视角灵敏度倍率、水平/垂直反转、移动死区倍率，以及移动端独立 `TouchLookSensitivityMultiplier（触控视角灵敏度倍率）` 和 `TouchMoveScale（触控移动幅度倍率）`。修改只写内存，调用 `SaveInputPreferences` 时才持久化。

## 8. 重绑定

```cpp
ListPlayerMappings();
PreviewRebind(RowName, Slot, Key);
ApplyRebind(RowName, Slot, Key);
ResetMappings(RowName);
SaveInputPreferences();
```

Touch/Gesture/连续轴不作为数字键位重绑定目标。冲突只预览，不隐式覆盖。

## 9. 移动端 Touch

```cpp
auto Handle = Input->BeginTouchInput(
    PointerId,
    EGamePlatformInputSemantic::Move,
    Owner,
    Result);

Input->UpdateTouchInput(
    Handle,
    FInputActionValue(FVector2D(X, Y)));

Input->EndTouchInput(Handle);
```

虚拟摇杆/按钮 Widget（控件）属于上层 UI，本插件只注入中立 InputActionValue（输入动作值）。

## 10. Snapshot（快照）

`GetInputSnapshot()` 返回：Profile/Binding/Mapping 状态、Gameplay 是否开放、设备族及 DeviceRevision、无障碍偏好、代次、阻断掩码、Context/Binding/Touch 数量和最近 Result（结果）。

`GetInputDiagnostics()` 返回当前 LocalPlayer 的轻量运行诊断：维护 Ticker 是否已安排、当前设备族及 DeviceRevision、Context/Block/Binding/Subscription/Touch 数量、事件发布/订阅回调/设备切换/Mapping重建/维护Tick/失效Owner回收计数，以及最近/最大维护耗时。诊断不包含原始按键、文字或Touch坐标。

Snapshot 仅是本地值，不构成服务器权威。