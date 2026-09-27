# Architecture（架构）

依赖链：

DivineBeastsPresentationRuntime → DivineBeastsRuntime + GamePlatformPresentationCore。

DivineBeastsPresentationClient → DivineBeastsPresentationRuntime + GamePlatformPresentationClient。

两模块均无MobaPresentation硬依赖。OpenWorld/Village事实可直接由DBAClient（项目客户端组合层）转换为项目表现事实，再进入GamePlatformPresentation（平台表现协调器）；Arena/Combat/Ability的MOBA事实仍由MobaPresentation转换语义后进入同一协调器。

本轮同时给GamePlatformPresentation补了跨游戏最小中立扩展：FGamePlatformPresentationContext（类型化上下文）、Context Contributor Registry（上下文贡献者注册表）、Catalog Fragment Registry（目录片段注册表）、Registration Handle（可撤销注册句柄）和确定性Resolver（解析器）。项目层不复制Dispatcher（分发器）或Provider Registry（提供者注册表）。
