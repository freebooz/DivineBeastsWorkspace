# GamePlatformInput（游戏平台输入）插件设计

> 更新日期：2026-09-27。
> 正式位置：`Game/Plugins/GamePlatform/Application/GamePlatformInput/`。
> 本文是插件职责、PC/移动端兼容、生命周期、性能和三层扩展边界的正式设计入口；公开接口见 [API.md](API.md)，验证证据见 [TestingAndEvidence.md](TestingAndEvidence.md)。

## 1. 定位

`GamePlatformInput` 是纯客户端平台输入层，只负责把本地硬件/触控输入转换为稳定中立语义，并管理 `LocalPlayer（本地玩家）` 作用域的输入 Profile（配置）、Mapping Context（映射上下文）、绑定、阻断租约、重绑定和非权威偏好。

依赖方向：

```text
DivineBeasts（神兽联盟项目层）
        ↓
GamePlatformUI / Character / Ability 等客户端消费者
        ↓
GamePlatformInputClient（游戏平台输入客户端）
        ↓
EnhancedInput + GamePlatformData + GamePlatformCore
```

模块类型是 `ClientOnly（仅客户端）`，Dedicated Server（专用服务器）不应链接本模块。

## 2. 负责与不负责

负责：

- `ULocalPlayerSubsystem（本地玩家子系统）` 作用域输入服务。
- `UGamePlatformInputProfileDefinition（平台输入配置定义）`。
- 中立 Input Semantic（输入语义）和单位。
- Enhanced Input Mapping Context（增强输入映射上下文）租约。
- 精确 Action（动作）绑定与解绑。
- 多来源输入 Block（阻断）租约。
- PC 键鼠/手柄玩家重绑定。
- 移动端 Touch（触控）语义注入。
- 焦点丢失、中断、旧代次防穿透。
- 本地无障碍/舒适度输入偏好。
- 当前设备族提示状态。

不负责：

- 服务器权威技能、移动或战斗结果。
- RPC（远程调用）发送。
- 具体英雄技能编号。
- UMG/CommonUI（界面）虚拟摇杆布局。
- 触控手势视觉识别 UI。
- Camera（相机）最终旋转积分。
- Gameplay Ability（玩法能力）激活授权。
- 全局 Player0（第零玩家）或进程单例输入。

## 3. PC端模型

### 键盘/鼠标

- Mapping Context 由 Enhanced Input 原生资产定义。
- `Move` 输出归一二维轴。
- `LookDelta` 将原始相对增量转换为 `DegreesDelta（角度增量）`，不乘 DeltaTime。
- Action 类语义输出 Boolean（布尔）请求。
- 玩家重绑定只允许离散键/鼠标按钮，不允许 Touch/Gesture/连续轴作为数字键位。

### 手柄

- `Move/LookRate` 使用二维模拟轴。
- Profile 的 `AnalogDeadZone（模拟死区）` 与本地 DeadZoneMultiplier（死区倍率）统一处理。
- `LookRate` 输出 `DegreesPerSecond（角速度）`，由最终 Camera 消费层只乘一次 DeltaTime。
- 玩家映射冲突按设备类型区分，键鼠与手柄不会被错误视为同一冲突域。

PC 最近设备类型默认 KeyboardMouse（键鼠）；手柄/键鼠切换由项目的真实平台/视口设备检测桥调用 `NotifyInputDeviceActivity` 上报，输入插件不为识别设备强行引入 UI 依赖。

## 4. 移动端模型

Profile 可以独立启用 `bEnableTouch（允许触控）`。移动端虚拟摇杆、技能按钮和触控手势通过：

```text
BeginTouchInput
    ↓
UpdateTouchInput
    ↓
Enhanced Input InjectInputForAction
    ↓
同一 Semantic / Action / Block / Receiver 链
    ↓
EndTouchInput
```

平台层不创建虚拟摇杆 Widget（控件），不绑定屏幕坐标，也不硬编码 Android/iOS UI。项目/UI 层只负责把触控结果转换为 `FInputActionValue（输入动作值）`。

首版移动端规则：

- PointerId（触点）限制 0..9。
- 同一触点只能有一个来源。
- 同一语义同时只接受一个 Touch 来源，避免两个虚拟控件争抢同一动作。
- Touch Move 使用独立 `TouchAnalogDeadZone（触控模拟死区）`。
- Touch 结束注入中性值，并中断对应语义，防止虚拟摇杆/按钮卡住。
- Touch 会自动把当前设备族更新为 `Touch（触控）`。
- 手势识别（单击/长按/滑动/双击）属于上层交互/UI；Input 只接收最终语义，不复制第二套手势状态机。

## 5. Input Profile（输入配置）

`UGamePlatformInputProfileDefinition` 继承 `UGamePlatformDefinitionBase（平台定义基类）`，通过 `GamePlatformData` 的 `Input` Asset Bundle（资产束）加载。

一个 Profile 包含：

- Actions：Semantic → UInputAction。
- Contexts：稳定上下文名 → UInputMappingContext。
- 鼠标/触控角度增量比例。
- 手柄角速度。
- 通用模拟死区。
- Touch 模拟死区。
- 键鼠/手柄/Touch 三类设备开关。
- 默认无障碍/舒适度输入设置。

Profile 基础校验不在输入事件回调中加载 UObject；实际 Action ValueType（动作值维度）在 Profile Data Lease（数据租约）完成后做第二阶段校验。

## 6. 中立语义与单位

现有平台语义：

```text
Move
LookDelta
LookRate
AttackPrimary
AbilitySlot1..4
Interact
TargetLock
Menu
Confirm
Cancel
```

这些只是本地请求语义，不是服务器权威协议。《神兽联盟》可以把十二生肖技能槽映射到这些平台语义，但不能把生肖类名写回 GamePlatformInput。

通道显式映射，不依赖枚举排列顺序：

- Move → Move Channel（移动通道）。
- LookDelta / LookRate → Look Channel（视角通道）。
- Menu / Confirm / Cancel → UICommands（界面命令通道）。
- 其他动作 → Actions（玩法动作通道）。

## 7. UI / Gameplay 输入仲裁

`AcquireInputBlock（申请输入阻断）` 采用多来源租约：

- Loading 可以阻断 Move/Look/Actions。
- Menu 可以阻断 Gameplay，但保留 Cancel/Menu UI 命令。
- TextEntry（文本输入）可以由组合层按需要申请多个通道。
- 一个来源释放不能清除另一个来源仍持有的阻断。

动作被阻断、焦点丢失、Receiver（接收器）变化或上下文异常移除时都会进入“待中性值重新武装”状态，防止菜单关闭后仍按住的键立即重新触发技能。

## 8. 重绑定和偏好

重绑定基于 UE5.8 `UEnhancedInputUserSettings（增强输入用户设置）`：

- `ListPlayerMappings` 枚举当前 Profile 行/槽。
- `PreviewRebind` 只读检查冲突。
- `ApplyRebind` 不隐式覆盖其他行。
- `ResetMappings` 可以重置单行或当前 Profile 全部登记行。
- `SaveInputPreferences` 只在显式保存时做磁盘 IO，高频输入路径绝不写磁盘。

本插件另外保存：

- LookSensitivityMultiplier（视角灵敏度倍率）。
- InvertLookX / InvertLookY（水平/垂直反转）。
- MoveDeadZoneMultiplier（移动死区倍率）。

这些都是本地非权威偏好。

## 9. 生命周期

Profile 生命周期：

```text
PrepareInputProfile
    ↓
GamePlatformData AcquireDefinition(Input Bundle)
    ↓
Validate Profile + loaded Actions/Contexts
    ↓
Register user settings contexts
    ↓
Acquire contexts / Bind receiver
    ↓
Input events
    ↓
ReleaseInputProfile
```

句柄包含 ScopeId + Id + Generation（作用域/身份/代次）；旧 LocalPlayer、旧 Profile 或旧绑定句柄不能操作新状态。

`PlayerControllerChanged（玩家控制器变化）` 会先中断旧 Gameplay 动作，再精确解绑旧 EnhancedInputComponent（增强输入组件），避免重生/切 Pawn 后旧输入继续生效。

## 10. 性能设计

高频路径原则：

- 不用每帧 Tick 轮询按键。
- Enhanced Input Action 回调驱动事件。
- 输入回调不加载资产、不写磁盘、不创建 Mapping Context。
- Task/Subscriber 回调期间禁止结构性修改，避免重入容器。
- Event 发布直接遍历固定上限订阅者，不为每个鼠标/摇杆样本复制订阅数组。

低频维护：

- 仅存在 Context/Block/Binding/Touch/Subscription 弱 Owner 时注册 0.25s / 4Hz 维护 Ticker。
- 空闲 LocalPlayer 无维护 Ticker。
- 维护 Ticker 只回收失效弱引用，不采样真实硬件输入。

容量安全上限：

- Context Lease：64。
- Block Lease：64。
- Input Binding：8。
- Event Subscription：32。
- Touch Pointer：最多10个，且每语义一个活动来源。
- Profile Actions：32。
- Profile Contexts：8。

这些是防无界增长的安全上限，不是最终 3A 性能预算。

## 11. 《神兽联盟》推荐组合

PC：

```text
Keyboard/Mouse + Gamepad
        ↓
Enhanced Input Mapping Context
        ↓
GamePlatformInput Semantic
        ↓
DBAClient / Character / Ability 消费
```

移动端：

```text
DivineBeastsUIClient 虚拟摇杆/按钮/手势
        ↓
Begin/Update/EndTouchInput
        ↓
同一 GamePlatformInput Semantic
        ↓
与PC共用后续消费链
```

这样 PC 与移动端只在“设备适配层”不同，角色/GAS/战斗消费逻辑不复制。

## 12. 当前已知边界

- 插件仍 `EnabledByDefault=false`，由 `GamePlatformUI` 等上层插件依赖时启用；主工程不需要为了平台模块化强制全局启用。
- 自动 PC 键鼠/手柄“最后使用设备”识别没有在平台 Input 内监听 Slate/UI；真实设备检测桥应调用 `NotifyInputDeviceActivity`。
- 当前工程尚无正式 Input Profile、InputAction、InputMappingContext `.uasset` 内容资产。
- 物理 Android/iOS 设备、屏幕安全区、虚拟摇杆手感、震动/陀螺仪尚未进行真机验收。
- Server（专用服务器）不包含该 ClientOnly 模块。

## 13. 验收原则

模块编译、原生策略测试、UE Automation、真实键鼠/手柄、Android/iOS 真机、设置重启恢复、UI/Gameplay 仲裁和 Client Cook 是不同证据，不互相冒充。