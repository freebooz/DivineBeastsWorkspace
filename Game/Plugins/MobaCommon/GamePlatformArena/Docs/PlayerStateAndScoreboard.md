# PlayerStateAndScoreboard（玩家状态与记分板）

`AGamePlatformArenaPlayerState（竞技玩家状态）` 公开复制Team、Hero、Ready、Kills、Deaths、Assists、Score、ObjectiveScore、ConnectionState、ForfeitState和StatsRevision。

`UGamePlatformArenaViewModel（竞技客户端视图模型）` 只读这些复制事实，并按TeamId/PlayerId稳定排序形成记分板，不拥有修改比分、胜负或结果的接口。