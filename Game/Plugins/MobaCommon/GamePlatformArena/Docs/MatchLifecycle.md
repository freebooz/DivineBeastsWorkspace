# MatchLifecycle（比赛生命周期）

阶段由 `FGamePlatformArenaMatchStateMachine（竞技阶段状态机）` 集中控制：Uninitialized → WaitingAssignment → Preparing → WaitingPlayers → HeroSelection → ReadyCheck → Countdown → InProgress → Ending → ResultPending → Completed。

Aborted/Failed仅走显式终止路径。禁止业务代码散落直接赋值阶段。比赛只能第一次合法结束，第二次结束不会再次构造或提交MatchResult。