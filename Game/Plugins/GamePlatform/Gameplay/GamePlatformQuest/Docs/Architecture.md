# Architecture（架构）

`GamePlatformQuest（游戏平台任务插件）`位于 `GameFoundation/Gameplay`，保持三个真实模块：`GamePlatformQuest（任务共享Runtime模块）`、`GamePlatformQuestClient（任务客户端ClientOnly模块）`、`GamePlatformQuestServer（任务服务器ServerOnly模块）`。

共享模块只包含 Quest Definition（任务定义）、Objective Definition（目标定义）、状态/快照/事件契约、设置、OwnerOnly StateComponent（仅拥有者状态组件）和状态机，不访问 HTTP/数据库。Client 只做快照缓存、Revision保护和 Track/Untrack；Server 做事件索引、Objective评估、去重、状态转换、批量持久化和 Persistence Port（持久化端口）。

Combat（战斗）与 Interaction（交互）不依赖 Quest。跨领域适配位于 `DBAServer（神兽联盟项目服务器组合层）`，将服务器可信 Combat/Interaction/Region/Gameplay 事实转换为 `FGamePlatformQuestEvent（平台任务事件）`。

跨会话真源位于既有 Go `PlayerDataService（玩家数据服务） + PostgreSQL`。没有新增 QuestService（任务独立微服务），也没有把永久任务进度只留在 UE 内存。
