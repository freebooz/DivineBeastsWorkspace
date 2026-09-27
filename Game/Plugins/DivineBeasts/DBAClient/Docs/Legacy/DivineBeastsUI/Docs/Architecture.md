# Architecture（架构）

DivineBeastsUIClient（神兽联盟项目UI客户端模块）是唯一模块，ClientOnly（仅客户端）。

依赖方向：

DivineBeastsUIClient
→ DivineBeastsRuntime（项目核心）
→ GamePlatformUIClient（平台UI客户端）

禁止直接依赖：

- DivineBeastsApplicationFlowClient（项目应用流程实现）
- DivineBeastsArenaClient（项目竞技客户端实现）
- DivineBeastsOpenWorldRuntime（项目开放世界运行时）
- DivineBeastsVillageRuntime（项目新手村运行时）
- Backend Contracts（后端契约）
- MatchService / PlayerDataService / GameServerControlService（后端服务）

业务实现与UI的连接放在 DBAClient（项目客户端组合插件）的 UDBAUICompositionSubsystem（UI组合根适配器）中。组合层可同时依赖业务Owner和UI Contract（契约），从而保持UI插件依赖方向单向。

Server Target（服务器构建目标）显式禁用 DivineBeastsUI；Dedicated Server（专用服务器）不得加载UI模块或UI资源。
