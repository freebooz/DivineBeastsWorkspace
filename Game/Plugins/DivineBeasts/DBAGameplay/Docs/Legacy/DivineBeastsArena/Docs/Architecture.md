# Architecture（架构）

DivineBeastsArena建立在MobaCommon/GamePlatformArena（MOBA通用竞技层）之上，不重写GameMode/GameState/PlayerState/Phase/Assignment/Ticket/Result框架。

依赖方向：
DivineBeastsArenaRuntime → DivineBeastsRuntime + DivineBeastsCharactersRuntime + GamePlatformArena/MobaData/MobaCore。
DivineBeastsArenaClient → Runtime + GamePlatformArenaClient + DivineBeastsApplicationFlowClient。
DivineBeastsArenaServer → Runtime + GamePlatformArenaServer + GamePlatformArena + DivineBeastsCharactersRuntime。

GamePlatformArenaServer新增中立IGamePlatformArenaServerProjectExtension（竞技服务器项目扩展接口），平台只调用接口，不依赖项目类。项目Server模块通过Modular Feature注册实现。

Combat、Characters、ApplicationFlow都不反向依赖DivineBeastsArena。
