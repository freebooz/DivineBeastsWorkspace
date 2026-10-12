# Task 4：Arena接口与VFX诊断实施报告

状态：源码已实施，独立审查发现的两项VFX缺陷已作限定源码修补，待主代理复核修补及统一UE编译/Automation；不提交/推送。

## 设计与文件
- GamePlatformArenaServer的BackendClient、Coordinator头从Public移至Private对应目录，去掉内部DLL导出宏。内部include文件名保持，公共项目扩展契约不变。HTTP/Json转私有，未使用的JsonUtilities移除；Docs/API.md同步。
- VFX新增Private/Diagnostics/GamePlatformVFXRuntimeCountRegistry.h：仅FGuid与计数，不持有World。WorldSubsystem初始化注册、关闭时立即注销并使ID无效，关闭后更新不复活记录。原有事件式统计调用继续使用。
- 全局Tracked/Pending为存活世界合计；Peak为模块生命周期内同时在场总数峰值，普通世界退出不清峰值，Module Shutdown重置。以int64累加并饱和到非负int32；按世界重算，避免饱和后减法丢失其他世界贡献。
- Private/Tests/GamePlatformVFXDiagnosticsTests.cpp现有三个诊断行为回归。Module、WorldSubsystem与Diagnostics既有文件接线；原实例与Data租约所有权没有迁移。

## 新测试完整名称
1. GamePlatform.VFX.Diagnostics.MultiWorldCounts：双世界/部分更新/退出/迟到更新/峰值/负输入/溢出/Reset。
2. GamePlatform.VFX.Diagnostics.WorldSubsystemLifecycle：普通Editor复用World->GetSubsystem的引擎实例，以DestroyWorld驱动注销；仅Commandlet显式创建/关闭测试实例并注明证据边界。销毁一个世界不影响另一世界，保留强引用检查迟到Update/重复关闭。
3. GamePlatform.VFX.Diagnostics.ResourcePendingTransitions：复用真实Data服务与已存在的UE类软资源，经生产Play/资源释放/异步完成边界检查Pending即时发布；缓存元数据仅测试自有瞬态对象，不伪造生产定义资产或Niagara成功。

本任务尚无修改后UE执行或修改前RED记录；昂贵UE工作由主代理统一串行完成，不能把测试文件存在计为通过。

## 已进行的源码核对
Game全树实际符号搜索显示两个Arena具体类仅在同模块被消费；公共子系统头只有前置声明，HTTP/JSON头仅在Private后端实现。引擎SubsystemCollection.h确认使用FSubsystemCollection模板的构造合同。已有PCG未提交文件未被此任务修改。总体目录规划登记新建/移动文件。

## 审查与验证入口
Build/Game/BuildFoundation.ps1，Editor/Client/Server及竞技AdditionalPlugins；Automation RunTests GamePlatform.VFX.Diagnostics。审查不完整类型/导出边界、模块退出线程、重复Deinitialize及多PIE；真实执行日志与修复追加ExecutionProgress。

## 独立审查后的限定修补与剩余验证
详细发现、原始行号、修补文件、已执行源码核对及未执行命令见 task-4-review.md。P1修正CreateWorld后重复NewObject导致注册翻倍的测试夹具；P2将全部10处生产Pending增减收敛到私有AdjustPendingInstanceCount并即时发布快照。只读源码断言四项PASS，真实5.8头文件接口已核对；没有UE编译/Automation动态结果。普通Editor自动生命周期、Commandlet显式分支及真实Data异步资源完成均待主代理统一执行并复核；测试世界销毁与资源释放由夹具负责。
