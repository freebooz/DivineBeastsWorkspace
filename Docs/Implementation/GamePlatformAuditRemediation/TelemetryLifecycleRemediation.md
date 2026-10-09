# Telemetry自定义Sink生命周期与上下文边界整改报告

已重新冻结，时间：2026-10-09T12:45:00.602930+08:00；未提交、未推送、未运行UBT。工作区：`C:\Users\Freebooz\.codex\worktrees\gameplatform-review-fixes\DivineBeastsWorkspace`。

| 真实问题 | 修复 | 回归源码 |
| --- | --- | --- |
| Start内关闭后外层安装候选 | 先登记配置门闩、捕获实例/Sink代次，外部Start后复核；失败/关闭候选Shutdown清理一次，不替换旧Sink。 | StartClose，ConfigureNested的Start失败/同对象Stopped拒绝 |
| Shutdown内重配置/启用/记录复活服务 | Deinitialize先永久closing/代次失效/取消调度/摘Sink；公开控制/新记录拒绝关闭，finalDrain仅内部绑定退休Sink，重入Deinit幂等。 | ShutdownReentry验证回调内Configure=false、事件Disabled、无Ticker、候选不Start和最终Drain；ExternalClose验证旧Flush关闭 |
| GetHealth换B后旧A继续出队Submit | 持活实际Sink副本，GetHealth后先复核成员身份/代次再BuildBatch；Submit后再次复核，旧栈不能消费后继队列/诊断/调度。 | HealthReplacement断言A.Submits=0、队列保留、B可以转交；ExternalClose验证Submit关闭 |
| 旧/重复异步完成与Ticker影响后继作用域 | 完成以TAtomic门闩消费一次，非GT只投递GT；核实例与Sink代次再调度。Ticker另有取消/消费代次，过期不能清新句柄。 | CompletionGeneration验证A旧完成不调度B、当前完成可调度、重复不再次调度 |
| EndSession/BeginSession/BeforeWorldTravel错误地用Sink换代取消账号/世界边界，留下旧身份；单独移除检查又会覆写真实后继上下文。 | ContextOperationGeneration独立保护Begin/End/Travel/UpdateWorldContext；Begin旧会话结束与新会话发布共用身份。外部Flush后核closing/实例/上下文操作，Sink换代仍完成边界，真实后继接管则停止旧发布。边界调用期间保活UObject。 | ContextSinkReplacement含End/Begin/Travel三种GetHealth同步ConfigureSink(B)；ContextSuccessor含End->Begin、Begin->End、Travel->UpdateWorldContext、Begin->Travel四种真实后继接管。 |

## 实际验证与边界

追加修复后实际执行Telemetry架构静态门禁与指定范围diff检查，退出码均为0；日志为ContextArchitecture.log、ContextDiffCheck.log。此前独立静态结果保留，完整命令见RepairReport.json。

八个实际UE Automation回归仅已写入源码；两次均先补测试源码再修实现，但分工禁止UBT，未执行修前红灯/修后绿灯。两个新增用例的七种场景直接调用真实Subsystem和可同步重入IGamePlatformTelemetrySink；没有复制生产算法或伪装联网。

独立复核所指三处实际错误已核源码：旧End/Begin/Travel会在GetHealth换Sink后返回。两项新增用例先写入源码，再实施修复；由于没有运行UE，不声称这些用例编译或通过。

## 兼容与中文审核

- 无新增公开方法或协议/反射身份迁移；Private布局与实现变化，消费者随统一UBT重编译。
- 关闭后同一UObject永久拒绝Initialize/Configure/Enable/Record；后继GI必须新实例，控制入口仅已Initialize作用域可用。
- 正在配置（Start及旧Flush/Shutdown）时嵌套Configure现在明确false，测试/自定义Sink需服从合同；返回false不表示旧已安装Sink被成功替换。
- 最终Drain不可恢复Ticker，旧Flush中发生关闭时不递归Drain；未转交缓冲按关闭策略丢弃，不保证远端落盘。
- 新测试仅WITH_DEV_AUTOMATION_TESTS友元撤销调度前置，不扩展生产公共可变接口。
- 单纯更换Sink不再跳过会话结束、新会话或旅行清理；最近真实Begin/End/Travel/UpdateWorldContext拥有边界发布权，旧外部Flush返回栈停止。普通关联/环境属性更新不取得边界身份。无公开API/协议变化，新增Private字段/方法随统一构建重编译。

人工抽检本轮GI永久关闭、候选和退休Sink所有权、调用前提/返回/嵌套拒绝、代次和异步线程、最终Drain取消/丢弃语义、测试前置与失败意义、README历史验证边界。 追加人工审核上下文/输出器/采样会话三种代次的区别、边界发布权与后继优先合同、Begin内部结束、三种换Sink与四种上下文后继测试前置/期望；README及SinksAndExporters保持一致。

未全量审核遥测全部历史实现或全工作区中文合规；静态扫描不替代人工审核。

根新增的完整中文公开API、外置默认/FVTableHelper构造与析构保留；不改缓冲、Schema、Transport或其他组文件。

## 修改与新增文件

- `Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Public/Subsystems/GamePlatformTelemetrySubsystem.h`
- `Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Private/Subsystems/GamePlatformTelemetrySubsystem.cpp`
- `Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/README.md`
- `Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Docs/SinksAndExporters.md`

本轮追加没有新增/移动源码文件；此前唯一新增源码仍需根同步总体目录规划：

- `Game/Plugins/GamePlatform/Diagnostics/GamePlatformTelemetry/Source/GamePlatformTelemetry/Private/Tests/GamePlatformTelemetryLifecycleTests.cpp`

新增私有方法：IsContextOperationCurrent、EndSessionInternal；新增私有字段：ContextOperationGeneration。没有新增公开API。源码实际哈希仅用于本次冻结定位，见JSON。

## 未验证与生产条件

- UBT/UHT及八个UE Automation回归（新增两项/七种场景包含在内）
- 跨GI/多PIE、真实网络关闭/重试、后端Ingest/NATS、Cook/Stage
- 关闭秒预算/CPU/内存与GT延迟实测
- 现有NetworkSink/Transport与后端未知合同不由本轮伪造；没有新生产Provider或真实联网测试。
- 原生缓冲/Schema/响应预算算法本轮未改，未重复执行其CMake回归；本轮静态命令和UE待验证边界独立记录。
