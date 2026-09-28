# GamePlatformInput实际进度

## 首个检查点：2026-09-21

## 2026-09-27 当前审查与完善

### 第二轮性能收敛执行计划与完成情况

### 第三轮语义分层与神兽联盟项目输入执行计划/完成情况

1. 平台语义解耦：新增 `FGamePlatformInputSemanticId / Descriptor`，旧固定枚举保留兼容，不继续增加项目技能。
2. Profile编译：新增 `InputProfileCompiler`，Profile准备时一次生成 `CompiledActions[CompactSlot]`、Tag→Slot低频索引和LegacyEnum→Slot兼容索引。
3. 高频运行时：Enhanced Input绑定直接捕获Slot；Route/Interrupt/Touch Update/End均按Slot数组访问，不在高频路径查GameplayTag/TMap。
4. 项目语义上移：新增 `DBAClient/DivineBeastsInputClient` 模块，定义 `DivineBeasts.Input.*` 主攻击、四技能槽和TargetLock语义；项目Profile禁止继续使用平台旧攻击/技能/锁定枚举。
5. 能力输入边界：项目攻击/技能语义映射到 `Platform.Ability.Input.DivineBeasts.*`；TargetLock不映射为GAS技能。`UGamePlatformAbilitySystemComponent` 已实现公开 AbilityInputReceiver 合同，项目层通过 Token + InputTag 驱动按下/持续/释放，不遍历或修改 GAS 私有 Spec。
6. 移动端：项目Touch入口改用 `BeginTouchInputBySemantic`，PC和移动端进入同一CompactSlot和项目事件链。
7. 验证：Native Debug/Release各410断言通过；当前正式工程已通过 `GamePlatformInputClient` 与 `DivineBeastsInputClient` 的 UE5.8 Editor/Win64 Client 定向模块构建，UHT/编译/链接均成功；输入专项架构脚本与三层继承边界门禁通过。
8. Built-in公共合同：新增 `EGamePlatformBuiltInInputSemantic` 与 `GetBuiltInSemanticTag/GetBuiltInSemanticDescriptor`，平台新代码只使用7个跨游戏公共语义；旧固定枚举完整保留为兼容层。
9. 私有职责拆分：设备默认/回退逻辑迁入 `Private/Devices/InputDevicePolicy.h`，继续保持单一平台LocalPlayerSubsystem，不机械拆多个Subsystem。
10. 联调缺口修复：验证输入→AbilitySystem链时发现 `UGamePlatformAbilitySetDefinition::ValidateDefinition()` 只有声明没有实现，补充纯字段校验后通过 UE5.8 Editor/Win64 Client AbilitySystem 定向构建；没有借此扩展GAS业务职责。
11. 最终边界：`GamePlatformInputClient`、`GamePlatformAbilitySystem`、`DivineBeastsInputClient` 的 Win64 Client 定向构建均成功；项目输入架构门禁通过。全局 `ValidateDesignBaseline` 仍有 DBAArena→GamePlatformUIClient 依赖声明问题，属于竞技插件外部阻断，不作为输入任务失败或通过证据。

1. 高频阻断判断：已将 BlockLedger 从每事件扫描最多64租约改为低频引用计数 + 缓存位掩码，高频 `IsBlocked` 为 O(1)。
2. 动作门禁：已将13个稳定语义的 ActionGate 从哈希节点改为固定数组槽，避免高频哈希和首次分配。
3. 跨端设备默认：Android/iOS 默认 Touch，桌面默认 KeyboardMouse；触屏PC不再因 `SupportsTouchInput` 被误判为移动端。
4. UI提示版本：新增 DeviceRevision，设备族真实变化才递增；重复上报不触发Mapping重建或版本变化。
5. 验证：Native Debug/Release 各1/1、410断言通过；UE5.8 Editor/Win64 Client模块再次构建成功；UE Automation实际尝试但被引擎全平台SDK校验（Android r27c/VisionOS MainVersion）阻断在测试队列前。

- 重新扫描真实插件后确认：原 HEAD 已有 Public 契约、Profile、语义和测试源，但从未存在 `Private/Policy/InputPolicy.h` 与 `Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp`，因此旧“核心基础已实装”实际只覆盖接口基础，不能代表运行实现完成。
- 本轮已补齐无引擎对象的输入策略内核和 LocalPlayer 执行层：Profile/Data租约、Enhanced Input Context租约、动作绑定、阻断租约、重绑定、Touch注入、焦点/Receiver生命周期、本地偏好和设备族状态。
- PC 支持键盘/鼠标与手柄；移动端统一通过 Touch 语义注入支持虚拟摇杆/视角/技能按钮。虚拟控件和手势UI仍归 GamePlatformUI/DivineBeastsUI，不让 Input 反向依赖 UMG/项目层。
- 移动端进一步支持独立 `TouchLookSensitivityMultiplier` 和 `TouchMoveScale`；长按/连点/滑动等交互继续由 Enhanced Input Trigger 或UI手势适配实现，平台Input不增加没有运行路径的假无障碍开关。
- 性能上采用事件驱动，无固定每帧输入 Tick；只有弱Owner租约存在时才使用4Hz维护Ticker回收失效记录。Context/Block/Binding/Subscription/Touch均有明确容量上限，高频事件不加载资产、不写磁盘。新增实例级Diagnostics记录事件/订阅回调、设备切换、Mapping重建、维护Tick、失效Owner回收和维护耗时。
- Native C++17 Debug／Release 各1/1通过，当前410条断言、0失败；覆盖Touch移动倍率/独立灵敏度以及BlockLedger缓存掩码重叠/乱序释放。
- UE5.8 `DivineBeastsArenaEditor Win64 Development -Module=GamePlatformInputClient` 与 `DivineBeastsArenaClient Win64 Development -Module=GamePlatformInputClient` 已实际构建成功。
- Android Client 构建已实际尝试，但 UBT 报 `Sdk: not found. Required version r27c`，当前 Runner 未满足移动工具链前置，未进入 C++ 编译；iOS 仍需要 macOS/Xcode/远程 iOS 工具链，不能在本 Windows Runner 上冒充验证。

当前路径为唯一工作空间，分支main；本次首查HEAD=f5c9d4a，存在并行PCG/World等修改与外部提交，实施不重置、不批量提交。根规则生效，受影响树内未发现局部AGENTS或覆盖文件。

已读现行中文总体规划和插件规范；附件所述OverallPlan.md不存在，正式对应文件为Docs/Architecture/解决方案总体规划.md。附件已内含36组验收与15类文档细则，无需臆造独立附件已读取。

已有Core/Data/Flow/Online/Session/Loading/World及PCG部分实现；Session尚无真实公开UE准入链，PCG尚无真实生成交付。引擎5.8.0、PCG1.0、EnhancedInput1.0，MSVC19.51/CMake4.3.2；尚未证明UE项目可编译。前序三目标在其他既有空白插件描述扫描失败，不能删文件或关闭模块制造通过。

主工程没有自研PlayerController/Pawn输入绑定，未发现Legacy动作映射、CommonUI视口或本地键位持久化。DefaultEngine仅已有GameInstance和AssetManager配置；不得覆盖为全局输入配置。引擎EnhancedInput默认启用，但实际PlayerInput/InputComponent类型仍须运行核验。

本任务范围：唯一GamePlatformInput插件、GamePlatformInputClient客户端模块；依赖Core/Data/EnhancedInput及必要引擎模块。主工程客户端胶水负责Loading/World/Session事实，不使输入反向依赖它们。不新建Go服务，不开放端口，不使用生产账户或数据库。

## 实施步骤

1. 核对原生映射、绑定、设置与保存接口；先运行所有权/抑制/重新武装失败测试。
2. 实施公开配置与句柄、LocalPlayer私有服务、原生映射和精确绑定、结果准备事实。
3. 实施重绑、隔离本地偏好与有限触摸源，失败明确报告；未提供能力不返回假成功。
4. 主工程显式开发输入适配和引擎资产脚本；保护服务器目标与既有默认地图/视口。
5. 执行可用原生/脚本/三目标检查，分别记录真实UE映射、设备、磁盘重启、网络、Cook缺口。
6. 从最终符号编写15份中文说明、36组追溯和待人工审查记录。
