# ArenaMatchmakingUI（竞技匹配界面）

项目UI知道五个正式竞技模式：

- Arena.Mode.Duel1v1
- Arena.Mode.Team2v2
- Arena.Mode.Team3v3
- Arena.Mode.Team4v4
- Arena.Mode.Team5v5

模式清单来自 DivineBeastsArenaClient（项目竞技客户端）在DBAClient组合层的公开Project Mode Catalog，不由UI自己硬算TeamSize。

当前 ArenaClient 只有MatchmakingRequest构建能力，没有正式匹配提交/取消传输端口，因此：

- AvailableArenaModeIds可以展示。
- bMatchmakingTransportAvailable=false。
- Start/CancelMatchmaking命令返回 Arena.MatchmakingTransportUnavailable。
- UI不能伪报“已入队”。

Party View（组队视图）当前没有项目UI-safe Party快照端口，状态未执行。

Pick/Ban和Duplicate Hero策略仍未批准，UI不擅自实现。
