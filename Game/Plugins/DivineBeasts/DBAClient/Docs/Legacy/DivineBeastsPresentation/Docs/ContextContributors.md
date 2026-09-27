# ContextContributors（上下文贡献者）

GamePlatformPresentation新增IGamePlatformPresentationContextContributor（平台表现上下文贡献接口）。

每个Contributor必须提供Stable ID（稳定编号）、Priority（优先级）、Scope（作用域）、ConflictPolicy（冲突策略）和Patch（补丁）。

平台按Priority与ContributorId确定性排序；ConflictPolicy支持FillMissing（只补空值）、Override（覆盖）和RejectConflict（冲突失败）。DivineBeasts项目Contributor编号为DivineBeasts.ProjectPresentation.Context，作用域LocalPlayer（本地玩家），策略RejectConflict。

World Cleanup会自动清理World作用域注册；项目Client还在World/Character切换、账号切换和Content Pack卸载时撤销相应句柄/逻辑预加载。Stale Handle（过期句柄）注销安全失败。
