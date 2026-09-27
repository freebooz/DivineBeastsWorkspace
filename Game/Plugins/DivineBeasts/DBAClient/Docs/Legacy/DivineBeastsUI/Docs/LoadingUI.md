# LoadingUI（加载界面）

加载投影直接订阅 GamePlatformLoadingClient（平台加载客户端）真实Snapshot（快照），不是用假计时器从0递增到100%。

当前映射的真实任务：

- SessionAdmission（会话准入）
- ExpectedWorld（目标世界）
- ExpectedExperience（目标体验）
- CharacterBinding（角色绑定）
- GameplayData（玩法数据）
- ProjectReadiness（项目就绪）

Progress（进度）= Ready任务数 / 总任务数；没有真实Task时Progress=-1，UI应显示不确定进度而不是伪造百分比。

UDivineBeastsUIClientSubsystem把项目Loading投影同步为 GamePlatformLoadingScreenService Token（平台加载界面令牌），Acquire/Update/Release全部走平台服务。

Map Loaded（地图加载）不等于Ready。
