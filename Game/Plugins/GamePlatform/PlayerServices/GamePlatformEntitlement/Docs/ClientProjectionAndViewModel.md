# ClientProjectionAndViewModel（客户端投影与显示模型）

客户端模块以 LocalPlayer 范围持有 Snapshot，不使用进程全局单例。所有 HasEntitlement、HeroUnlocked、SkinUnlocked 都是只读投影。

客户端仅调用 Gateway GET Snapshot，不包含 Grant/Revoke、PlayerData Internal Token、X-Game-Server-Id 或数据库能力。

当前没有创建正式 Widget（界面组件）。UI 后续可消费 Snapshot 与查询结果，但不应每帧访问后端。