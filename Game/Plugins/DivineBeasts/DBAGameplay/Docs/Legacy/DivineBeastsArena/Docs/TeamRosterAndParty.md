# TeamRosterAndParty（队伍名单与组队）

Party由MatchService后端生成/维护；Arena不重新组队。

可信MatchmakingTicket的PartySnapshot现在携带PlayerIDs + CharacterIDs，CreateArenaMatch原子地把同Party成员放入同一Team，并生成Assignment Roster。

项目Arena只校验：
- ArenaMode对应TeamSize。
- Roster总人数和两队人数。
- PlayerId唯一。
- CharacterId唯一。
- Team分配与后端Assignment保持不变。

项目Arena不得重排后端Team，也不得用HeroDefinitionId代替CharacterId。
