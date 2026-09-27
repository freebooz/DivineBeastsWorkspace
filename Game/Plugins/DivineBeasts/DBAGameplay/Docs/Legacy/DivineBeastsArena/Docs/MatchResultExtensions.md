# MatchResultExtensions（比赛结果扩展）

MatchResult Extension Gate：不适用。

现有FGamePlatformArenaMatchResult / Go match.MatchResult已包含MatchId、ArenaModeId、GameServerId、时间、WinningTeam、EndReason、Team Score、Player K/D/A/Score、EndRevision，当前没有业务证据要求项目额外字段。

本轮修复了身份错误：
- Assignment Roster新增CharacterId。
- TransferTicket绑定CharacterId。
- UE PlayerState区分CharacterId与HeroDefinitionId。
- MatchResult写真实CharacterId。
- 后端提交时用Assignment校验PlayerId/CharacterId/TeamId一致性。

Submit仍复用GameServerControlService现有Result接口和Match.Completed Outbox。
