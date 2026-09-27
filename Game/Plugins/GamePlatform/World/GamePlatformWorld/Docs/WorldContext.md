# 世界上下文字段来源与有效性

| 字段 | 来源 | 权威与空值 |
|---|---|---|
| WorldId | 已加载WorldDefinition.LogicalId | 地图改名不改变逻辑身份，不自动证明服务器分配 |
| ExperienceId | DefaultExperienceId | 可空，不补正式业务默认值 |
| ServerRole | 显式专服开发装配 | 仅开发；离线客户端为空 |
| ServerInstanceId | 待真实Session/控制面绑定 | 本轮保持空，不从本地GUID伪造 |
| ServerStartGeneration | 待控制面提供 | 0表示未提供 |
| ShardId | 待权威分配 | 本轮空；Region不是Shard |
| RegionId | 恰好一个明确观察者的空间查询 | 多观察者为空，不冒充所有玩家当前位置 |
| BuildVersion | 显式项目装配并校验的项目版本 | 不混同DataVersion或UE版本 |
| MapPackageName | 当前UWorld包名去PIE前缀 | 与定义软引用包严格匹配 |
| ContextGeneration | 每个UWorld初始化生成的GUID | 不是服务器启动代次；跨图变化，不复用 |
| AuthorityKind | 运行模式检查 | DevelopmentLocal/DevelopmentServer不是生产Authority |
| ReadinessState/Result | 当前事实聚合/错误 | 未初始化和默认结果不表示成功 |

快照是值副本。世界退出后旧副本可用于历史诊断，但不得继续当作当前Ready。异步定义加载回调绑定弱子系统和预期GUID；旧世界销毁、关闭或目标变化直接拒绝更新。网络世界初始化保持显式前置错误，直到Session公开真实快照可用。

