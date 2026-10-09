# GamePlatformInput（游戏平台输入）

`GamePlatformInput` 是 ClientOnly（仅客户端）的跨游戏输入平台插件。它使用 UE5.8 Enhanced Input（增强输入），以 `ULocalPlayer（本地玩家）` 为作用域管理可扩展 SemanticId/Descriptor（语义标识/描述）、Profile编译、CompactSlot（紧凑槽位）、Mapping Context（映射上下文）、动作绑定、阻断租约、重绑定、Touch（触控）注入和本地无障碍偏好。

2026-09-27 已完成本轮跨端输入补强：生产策略层和 LocalPlayer 执行层保持事件驱动，新增移动端独立 Touch 视角灵敏度／移动幅度倍率和轻量 Input Diagnostics（输入诊断）。Native Debug/Release 当前各1/1通过，共410条断言；UE5.8 Editor 与 Windows Client 的 `GamePlatformInputClient` 模块编译通过。Android 构建已实际尝试，但当前 Runner 缺少 UE5.8 要求的 NDK r27c，停在 SDK 校验阶段；iOS 仍需 macOS/Xcode 远程或真机工具链。

设计入口：[插件设计](Docs/Architecture.md)｜[Semantic模型](Docs/SemanticModel.md)｜[Public API说明](Docs/API.md)｜[测试与验证证据](Docs/TestingAndEvidence.md)｜[人工审核](Docs/ManualReview.md)。

## 关键原则

- 平台语义标签与Development测试标签均在引擎初始化后的游戏线程首次使用时读取；禁止全局静态初始化调用`UGameplayTagsManager`。单体Client的CRT阶段没有UObject环境，编辑器DLL模块晚加载会掩盖这一类错误。标签配置和Legacy序列化身份保持不变；Cook客户端初始化回归必须与Editor输入合同测试分别验证。

- PC键鼠、PC/外接手柄、Android/iOS Touch 共用同一 Semantic（语义）消费链。
- 新项目语义使用稳定GameplayTag Descriptor并在Profile准备阶段编译为CompactSlot；旧固定枚举只保留兼容，禁止继续追加项目技能。
- 虚拟摇杆/技能按钮 UI 不进入平台 Input；UI 只通过 Touch API 注入中立动作值。
- 不用固定每帧 Tick 读取按键；真实输入由 Enhanced Input 回调驱动。
- 只有存在弱Owner租约时才使用4Hz维护Ticker做资源回收。
- 任何输入只是客户端请求，不构成服务器战斗、移动或技能权威。
- 不使用 Player0 全局假设；每个 LocalPlayer 完全隔离。
- 重绑定和磁盘保存只发生在菜单/显式保存路径，不进入高频输入事件。
- Touch 视角和移动手感可以独立于 PC 配置；触控值仍进入同一 Semantic（语义）链，不复制玩法逻辑。
- `GetInputDiagnostics()` 只暴露容量、事件计数和维护耗时，不记录原始按键、文字或触摸坐标。
- 高频 `IsBlocked()` 使用缓存阻断掩码 O(1) 判断；动作生命周期门禁使用固定语义槽数组，避免鼠标/摇杆高频回调中的租约扫描、哈希查找和隐式分配。
- Android/iOS 目标默认 Touch，桌面目标默认 KeyboardMouse；触屏 PC 不会仅因支持触摸而启动为移动端提示。`DeviceRevision` 只在设备族真实变化时递增，供 UI 低成本刷新提示。
- 平台公共 Move/Look/Interact/UI 语义通过 `EGamePlatformBuiltInInputSemantic（平台内建输入语义）` 访问；旧 `EGamePlatformInputSemantic` 只做资产/API兼容，Attack/AbilitySlot/TargetLock 不再属于新平台合同。
- `DivineBeastsInputClient（神兽联盟输入客户端）` 使用 `DivineBeasts.Input.*` 项目语义，并通过平台 Ability Input Receiver（能力输入接收器）接 GAS；项目语义不会回灌 GamePlatform。

## 当前设备支持

```text
Windows PC
├── Keyboard / Mouse
└── Gamepad

Android / iOS
└── Touch
    ├── Virtual Move Stick
    ├── Virtual Look Stick / Drag
    └── Ability / Interact / Menu Buttons
```

手柄/键鼠自动“最后使用设备”检测由项目真实平台设备桥调用 `NotifyInputDeviceActivity`；Touch API 会自动上报 Touch。

## 当前未宣称

- 没有真实 Input Profile/InputAction/InputMappingContext `.uasset` 交付。
- 没有 Windows 键鼠/手柄实机人工签审。
- Android 模块构建已尝试，但当前 Runner 缺少 NDK r27c；Android/iOS 真机触控、屏幕适配和性能验收均未完成。
- 没有 Client Cook/Stage 证明。
- 没有把本地输入当作服务器权威。

## 2026-09-30设计审查修订

ResetMappings(None)只重置当前Profile声明且原生已登记的行；显式外部行拒绝。SaveInputPreferences的Success表示提交原生void保存，不证明落盘；bPreferencesSaveSubmitted表明已提交，bPreferencesSaved保留身份但当前保持false。磁盘成功/失败不可观测，前置失败明确返回。

本次真实源码/Native/静态检查与未执行UE/后端/Cook边界见Game/Saved/Reviews/task2-repair-report.md；旧历史运行证据不自动覆盖本次修改。
