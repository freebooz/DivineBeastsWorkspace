# BackendIntegration（后端联调）

新增业务后端接口：无。新增 AI Go 微服务：无。实时 AI Brain 权威：UE Dedicated Server（专用服务器）。

AI Perception、Target Selection、MoveTo、BehaviorTree节点和每次攻击都不访问 Go/数据库。项目内没有 HttpModule/PostgreSQL 实时依赖。

未来永久NPC状态、跨服世界Boss、长期任务状态或 LiveOps 配置如有明确需求，应归属 World Persistence/Quest/LiveOps/ServerControl 等正式领域，而不是为了保存 Blackboard 创建 AIService。

真实 Online/Session 链本身尚未完成可验证准入，因此“复用真实认证/Session进入AI专服”的后端联调状态为未执行。
