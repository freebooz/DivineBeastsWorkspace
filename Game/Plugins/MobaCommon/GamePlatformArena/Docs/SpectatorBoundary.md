# SpectatorBoundary（观战边界）

第一版不实现外部Spectator（观战）和Replay（回放）。普通非Roster玩家在比赛开始后仍拒绝加入；Roster玩家的再次进入按Reconnect（重连）处理。

死亡/淘汰后的本地观战表现可由上层客户端实现，但不得借此扩大为外部观战服务。