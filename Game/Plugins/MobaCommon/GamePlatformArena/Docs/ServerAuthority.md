# ServerAuthority（服务器权威）

MainArena Dedicated Server（主竞技场专用服务器）是Team、Hero锁定、Ready、MatchPhase、Spawn时机、Score、Winner和MatchResult的唯一实时权威。

客户端没有SetTeam、SetScore、SetWinner、SubmitResult、ChangeArenaMode或为其他玩家选人/Ready的RPC。PlayerController仅暴露自身的Hero Selection、Ready、Forfeit低频请求。