# ReconnectForfeitAndPostMatch（重连、弃权与赛后）

Reconnect和Forfeit继续使用GamePlatformArena通用生命周期。

Ticket现在绑定PlayerId + CharacterId + MatchId + DestinationServer，重新连接仍从原Assignment Roster恢复CharacterId和Team。

项目Arena不定义新的Forfeit算法。

比赛结果提交完成后：
Result committed → PostMatch → ApplicationFlow RequestPostMatchReturnToWorld → 请求新的OpenWorld Assignment → 新Ticket → Session transfer。

旧MainArena endpoint/ticket不得复用。
