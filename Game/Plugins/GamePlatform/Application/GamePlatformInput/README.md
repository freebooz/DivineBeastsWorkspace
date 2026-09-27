# GamePlatformInput（游戏平台输入）

`GamePlatformInput` 是 ClientOnly（仅客户端）的跨游戏输入平台插件。它使用 UE5.8 Enhanced Input（增强输入），以 `ULocalPlayer（本地玩家）` 为作用域管理 Input Profile（输入配置）、Mapping Context（映射上下文）、动作绑定、阻断租约、重绑定、Touch（触控）注入和本地无障碍偏好。

2026-09-27 审查确认原 HEAD 缺少实际 `InputPolicy.h` 和 `GamePlatformInputLocalPlayerSubsystem.cpp`；本轮已经补齐生产策略层和 LocalPlayer 执行层。Native Debug/Release 当前各1/1通过，共401条断言。UE5.8 模块构建结果见 [测试与验证证据](Docs/TestingAndEvidence.md)。

设计入口：[插件设计](Docs/Architecture.md)｜[Public API说明](Docs/API.md)｜[测试与验证证据](Docs/TestingAndEvidence.md)｜[人工审核](Docs/ManualReview.md)。

## 关键原则

- PC键鼠、PC/外接手柄、Android/iOS Touch 共用同一 Semantic（语义）消费链。
- 虚拟摇杆/技能按钮 UI 不进入平台 Input；UI 只通过 Touch API 注入中立动作值。
- 不用固定每帧 Tick 读取按键；真实输入由 Enhanced Input 回调驱动。
- 只有存在弱Owner租约时才使用4Hz维护Ticker做资源回收。
- 任何输入只是客户端请求，不构成服务器战斗、移动或技能权威。
- 不使用 Player0 全局假设；每个 LocalPlayer 完全隔离。
- 重绑定和磁盘保存只发生在菜单/显式保存路径，不进入高频输入事件。

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
- 没有 Android/iOS 真机触控和性能验收。
- 没有 Client Cook/Stage 证明。
- 没有把本地输入当作服务器权威。