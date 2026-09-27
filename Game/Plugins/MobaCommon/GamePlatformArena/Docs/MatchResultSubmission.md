# MatchResultSubmission（比赛结果提交）

MainArena服务器构建MatchResult后通过GameServerControlService提交。PostgreSQL以matchId为业务唯一键，并对结果内容计算SHA-256；同matchId同内容返回AlreadyCommitted，不同内容返回Conflict，禁止覆盖已提交结果。

首次提交与 `Match.Completed（比赛完成事件）` 写入 `gameplatform_outbox（平台事务外发表）` 在同一个数据库事务内完成。网络超时按Outcome Unknown处理，服务器使用同matchId、同内容有界重试。