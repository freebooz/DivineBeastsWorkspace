# Permissions（协议权限与权威边界）

- Game Client（游戏客户端）只能调用公开OpenAPI，不得提交权威MatchResult（比赛结果）、MMR（隐藏评分）或GameServer状态。
- Dedicated Server（专用服务器）可调用GameServerControl、ServerTransfer验证和MatchResult提交等受信RPC。
- `SubmitMatchResult` 仅接受可信MainArena实例调用。
- `TransferTicket（跨服迁移票据）` 必须短有效期、签名、一次性消费并绑定玩家、会话与目标服务器。
- `OpenWorld.Hub（大厅/主城体验）` 与 `OpenWorld.Main（开放世界主体验）` 均由 `GameServer.Role.OpenWorld` 承载。
- `Village.Tutorial（教学体验）` 与 `Village.Training（训练体验）` 均由 `GameServer.Role.Village` 承载，不得重新建立Tutorial/Training独立服务器角色。
- 1v1～5v5全部由 `GameServer.Role.MainArena` 承载，通过ArenaModeId（竞技模式编号）区分。
- Shared协议不得承载UE逐帧战斗实现；伤害、技能、Buff、位置和AI等实时权威逻辑属于UE Dedicated Server。
