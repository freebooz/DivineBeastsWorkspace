# AssignmentAndAdmissionBoundary（分配与准入边界）

Assignment由现有MatchService/GameServerControlService产生，不由客户端构造。

项目新增校验字段：
- ExperienceId。
- ProjectRuleRevision。
- ContentRevision。
- HeroCatalogRevision。
- CharacterId（每个Roster Slot）。

DivineBeastsArenaServer要求：
- Mode属于五模式。
- Role=GameServer.Role.MainArena。
- Experience=Experience.MainArena.Main。
- Map/TeamSize/TotalPlayers与项目Production Definition一致。
- Rule/Content/HeroCatalog Revision一致。
- Roster PlayerId/CharacterId唯一。

TransferTicket同样绑定CharacterId；票据CharacterId必须与Assignment Roster一致。
