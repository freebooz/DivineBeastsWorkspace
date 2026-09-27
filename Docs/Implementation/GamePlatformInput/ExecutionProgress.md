# GamePlatformInput实际进度

## 首个检查点：2026-09-21

## 2026-09-27 当前审查与完善

- 重新扫描真实插件后确认：原 HEAD 已有 Public 契约、Profile、语义和测试源，但从未存在 `Private/Policy/InputPolicy.h` 与 `Private/Subsystems/GamePlatformInputLocalPlayerSubsystem.cpp`，因此旧“核心基础已实装”实际只覆盖接口基础，不能代表运行实现完成。
- 本轮已补齐无引擎对象的输入策略内核和 LocalPlayer 执行层：Profile/Data租约、Enhanced Input Context租约、动作绑定、阻断租约、重绑定、Touch注入、焦点/Receiver生命周期、本地偏好和设备族状态。
- PC 支持键盘/鼠标与手柄；移动端统一通过 Touch 语义注入支持虚拟摇杆/视角/技能按钮。虚拟控件和手势UI仍归 GamePlatformUI/DivineBeastsUI，不让 Input 反向依赖 UMG/项目层。
- 移动端进一步支持独立 `TouchLookSensitivityMultiplier` 和 `TouchMoveScale`；长按/连点/滑动等交互继续由 Enhanced Input Trigger 或UI手势适配实现，平台Input不增加没有运行路径的假无障碍开关。
- 性能上采用事件驱动，无固定每帧输入 Tick；只有弱Owner租约存在时才使用4Hz维护Ticker回收失效记录。Context/Block/Binding/Subscription/Touch均有明确容量上限，高频事件不加载资产、不写磁盘。新增实例级Diagnostics记录事件/订阅回调、设备切换、Mapping重建、维护Tick、失效Owner回收和维护耗时。
- Native C++17 Debug／Release 各1/1通过，当前406条断言、0失败；新增Touch移动倍率单位圆约束和Touch独立视角灵敏度组合验证。
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
