# GamePlatformLoading：加载与可玩就绪屏障

本插件已写入任务图、实例服务、Data租约适配、基础世界屏障以及主工程Flow接线。**尚未完成UE编译与真实运行验收，不可作为生产准入或完整游戏就绪证明。** 会话适配因Session公开服务和真实准入链缺失而未实施；请求`SessionReady`明确失败，不发HTTP也不模拟成功。

单一`GamePlatformLoading` Runtime模块同时支持Editor、Client、Server。只依赖Core、CoreUObject、Engine、GamePlatformCore、GamePlatformData，不依赖UMG、Niagara、项目美术、Flow或客户端Session模块。插件默认关闭；正式主工程已显式启用。无需添加空Client/Server/Editor模块。

Loading拥有操作、任务尝试与屏障，不拥有主资产、旅行、认证、网络连接或正式世界流送。Data负责真实定义加载；Flow决定进入加载阶段；项目bootstrap声明基础世界可操作；将来Session只提供可校验的会话事实。

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

原生任务算法Debug/Release通过；三个正式UE目标均在历史空白插件描述扫描阶段失败。真实租约UE测试、Foundation场景、多PIE、Cook及人工签审均未执行。详见仓库`Docs/Production/GamePlatformLoadingVerification.md`，不得将39条原生断言换算成39项UE验收。
