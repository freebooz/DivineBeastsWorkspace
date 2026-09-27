# TestingAndEvidence（测试与证据）

当前静态自动化覆盖：

- 唯一ClientOnly模块与最小依赖。
- 无ApplicationFlow/Arena实现硬依赖。
- 平台UI Manager和Screen Definition复用。
- 无CreateWidget+AddToViewport业务旁路。
- 19个一期UI Surface清单。
- Layer/Input/Focus静态策略。
- UI Query Source / Command Port。
- Revision/PageGeneration旧回调防护。
- Login/Character/World/PostMatch Intent接线。
- 真实Loading Snapshot进度。
- 五种ArenaMode展示。
- Arena ResultPending/Committed分离。
- Matchmaking/HeroSelection未接Owner时Fail Closed。
- World HUD Health/Shield/Interaction事件驱动投影。
- Server Target UI剥离。
- 无直接HTTP/权威Gameplay修改。
- ErrorCode本地化。
- GamePlatformUI/CommonUI平台回归。
- 三层架构与DeveloperTools。

UE Automation源码还覆盖页面清单、命令契约、路由和本地化，但当前没有UE5.8工具链，因此Automation实际运行未执行。

Keyboard/Gamepad/Touch、Multi-PIE、Travel、Client/Server Cook和性能均需要真实环境证据。
