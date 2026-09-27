# ClientAdapter（客户端适配）

Client Module Gate：通过。

UDivineBeastsArenaClientSubsystem职责：
- 暴露项目五ArenaMode ID。
- BuildMatchmakingRequest前执行项目Production Validation。
- 注册DivineBeastsApplicationFlow竞技扩展点。
- 跟踪ApplicationFlow是否InWorld。
- PostMatch调用ApplicationFlow RequestPostMatchReturnToWorld。

不负责：
- Matchmaking算法/MMR。
- Server规则。
- Result提交。
- UI Widget。
- VFX/SFX。
- ClientTravel。

当前Production规则NotConfigured，因此BuildMatchmakingRequest会安全失败，不允许客户端排入未批准规则的正式比赛。
