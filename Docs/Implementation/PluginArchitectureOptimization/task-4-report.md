# Task 4：Arena接口与VFX诊断实施报告

状态：源码已实施，待统一UE编译、Automation和独立审查；不提交/推送。

## 设计与文件
- GamePlatformArenaServer的BackendClient、Coordinator头从Public移至Private对应目录，去掉内部DLL导出宏。内部include文件名保持，公共项目扩展契约不变。HTTP/Json转私有，未使用的JsonUtilities移除；Docs/API.md同步。
- VFX新增Private/Diagnostics/GamePlatformVFXRuntimeCountRegistry.h：仅FGuid与计数，不持有World。WorldSubsystem初始化注册、关闭时立即注销并使ID无效，关闭后更新不复活记录。原有事件式统计调用继续使用。
- 全局Tracked/Pending为存活世界合计；Peak为模块生命周期内同时在场总数峰值，普通世界退出不清峰值，Module Shutdown重置。以int64累加并饱和到非负int32；按世界重算，避免饱和后减法丢失其他世界贡献。
- Private/Tests/GamePlatformVFXDiagnosticsTests.cpp新增两个行为回归。Module、WorldSubsystem与Diagnostics既有文件接线；原实例与Data租约所有权没有迁移。

## 新测试完整名称
1. GamePlatform.VFX.Diagnostics.MultiWorldCounts：双世界/部分更新/退出/迟到更新/峰值/负输入/溢出/Reset。
2. GamePlatform.VFX.Diagnostics.WorldSubsystemLifecycle：真实UWorld与生产Initialize/Update/Deinitialize，注销不影响另一世界。夹具暂时清空旧StartupCatalogs隔离外部预载，不伪造Niagara成功。

本任务尚无修改后UE执行或修改前RED记录；昂贵UE工作由主代理统一串行完成，不能把测试文件存在计为通过。

## 已进行的源码核对
Game全树实际符号搜索显示两个Arena具体类仅在同模块被消费；公共子系统头只有前置声明，HTTP/JSON头仅在Private后端实现。引擎SubsystemCollection.h确认使用FSubsystemCollection模板的构造合同。已有PCG未提交文件未被此任务修改。总体目录规划登记新建/移动文件。

## 审查与验证入口
Build/Game/BuildFoundation.ps1，Editor/Client/Server及竞技AdditionalPlugins；Automation RunTests GamePlatform.VFX.Diagnostics。审查不完整类型/导出边界、模块退出线程、重复Deinitialize及多PIE；真实执行日志与修复追加ExecutionProgress。
