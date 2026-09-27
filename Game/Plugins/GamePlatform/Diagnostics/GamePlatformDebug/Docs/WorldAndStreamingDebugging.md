# WorldAndStreamingDebugging（世界与流送调试）

World Provider（世界状态提供者）读取：
WorldId（世界编号）、Map（地图）、NetMode/ServerRole（网络模式/服务器角色）、WorldGeneration（世界代次）、WorldReady（世界就绪）、StreamingLevels（流送关卡数量）、NavigationReady（导航就绪）。

ExperienceId（体验编号）、Region（区域）和 DataLayers（数据层）在 GamePlatformWorld（平台世界插件）尚未提供稳定公开接口时显示 N/A（不可用）。

World Partition（世界分区）不执行每帧全量 Cell（单元）扫描；第一版只保留轻量摘要。World Travel（世界切换）后旧 Target（调试目标）弱引用失效。