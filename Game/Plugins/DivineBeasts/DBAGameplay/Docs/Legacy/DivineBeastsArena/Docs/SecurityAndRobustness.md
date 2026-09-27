# SecurityAndRobustness（安全与健壮性）

权威边界：

- Client不能设置Team、Score、Winner、Phase、Mode。
- Client不能选择其他Player的Hero。
- Client不能绕过HeroEligibility。
- Client不能提交MatchResult。
- MatchService后端决定Roster/Team/GameServer。
- TransferTicket绑定PlayerId + CharacterId + MatchId + DestinationServer。
- UE Admission要求Ticket CharacterId与Assignment Roster一致。
- MatchResult后端再次校验PlayerId/CharacterId/TeamId与原Assignment一致。
- ProjectRuleRevision/ContentRevision/HeroCatalogRevision不匹配时Fail Closed。
- Production配置缺失时Fail Closed，不回退客户端或猜默认值。

Dev/Test配置不得用于Shipping。项目Server不保存后端Token到复制状态，不依赖客户端Entitlement判断，不直接发长期奖励/MMR。

旧Element/FiveCamp/Faction/Pantheon/KingSeal/破元/共鸣不得进入Arena规则、Tag或结果字段。
