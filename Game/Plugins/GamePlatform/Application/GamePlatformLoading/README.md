# GamePlatformLoading：加载与可玩就绪屏障

本插件已实现任务图、实例服务、Data租约适配、基础世界屏障、按需低开销调度、运行诊断以及主工程 Flow 接线。2026-09-27 原生策略 Debug/Release 各46条断言通过，UE5.8 Editor／Client／Server 的 `GamePlatformLoading` 模块构建均成功；**UE Automation、真实资产/世界运行、Session准入、Cook/Stage和人工签审仍未完成，因此不可作为生产准入或完整游戏就绪证明。** 请求 `SessionReady` 在真实 Session 公开服务接通前继续明确失败，不发HTTP也不模拟成功。

单一`GamePlatformLoading` Runtime模块同时支持Editor、Client、Server。只依赖Core、CoreUObject、Engine、GamePlatformCore、GamePlatformData，不依赖UMG、Niagara、项目美术、Flow或客户端Session模块。插件默认关闭；正式主工程已显式启用。无需添加空Client/Server/Editor模块。

Loading拥有操作、任务尝试与屏障，不拥有主资产、旅行、认证、网络连接或正式世界流送。Data负责真实定义加载；Flow决定进入加载阶段；项目bootstrap声明基础世界可操作；将来Session只提供可校验的会话事实。

性能策略：空闲 GameInstance 不注册 Loading Ticker；运行中最多20Hz采样任务和截止时间；Ready后若资源仍被持有，仅2Hz监视弱Owner失效；释放后完全停表。单操作最多256任务、单任务最多64直接依赖，TaskId运行期使用冻结索引；状态订阅与自定义任务工厂各最多128项。`GetLoadingDiagnostics()` 提供Ticker/Poll/快照/回调计数和最近/最大Tick耗时，便于后续性能回归。

## 最小使用

在已初始化GameInstance的游戏线程取得`IGamePlatformLoadingService::Get(Instance)`。传入包含Data和WorldPresence任务的操作，订阅快照，在真实目标世界已开始且项目输入/控制器满足后调用`ReportWorldOperable`。通过`IsReadyToPlay(Handle)`判断可用，不比较百分比。使用结束必须调用`ReleaseLoadingOperation`。

主工程显式`-FoundationStandalone`开发入口的`Ready`工厂已接入`UDBALoadingFlowNode`：真实加载`foundation.probe@1`与`foundation.flow@1`，同时等待Sandbox世界声明。这两份定义和地图必须由现有UE资产工具生成；本轮没有写假资产。命令见配置运行文档。

## 十二份人工审查材料

1. [README](README.md)：总说明与入口。
2. [架构](Docs/Architecture.md)：目录、依赖、实例和清理。
3. [API](Docs/API.md)：实际签名、参数、失败与示例。
4. [任务模型](Docs/TaskModel.md)：工厂、DAG和代次。
5. [屏障](Docs/ReadinessBarrier.md)：真实就绪与失败策略。
6. [进度](Docs/ProgressModel.md)：权重和显示限制。
7. [集成](Docs/Integration.md)：Data、Flow、Session与主工程。
8. [配置运行](Docs/ConfigurationAndRun.md)：构建、测试、运行与Cook。
9. [测试证据](Docs/TestingAndEvidence.md)：25项追溯。
10. [排障](Docs/Troubleshooting.md)：失败诊断。
11. [迁移交接](Docs/MigrationAndHandover.md)：后续能力接入边界。
12. [人工审查](Docs/ManualReview.md)：待人工签审表。

当前原生任务算法 Debug/Release 各1/1通过并输出46条断言；Editor／Client／Server 三个 `GamePlatformLoading` 模块构建均成功。真实租约 UE Automation、Foundation真实场景、多PIE、Session准入、Cook/Stage及人工签审仍未执行。2026-09-21 曾因历史空白插件描述在扫描阶段阻断三目标，该证据作为历史保留；最新状态见仓库 `Docs/Production/GamePlatformLoadingVerification.md`，不得把46条原生断言或模块编译换算成46项UE运行验收。
