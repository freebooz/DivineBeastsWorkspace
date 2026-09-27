# Troubleshooting（故障排查）

Assignment拒绝时先检查ArenaModeId、TeamSize、TotalPlayers、Roster两队人数和 `GameServer.Role.MainArena（主竞技场角色）`。Ticket拒绝时检查过期、目标Player/Match/Server和是否已消费。

ResultPending长期存在时检查GameServerControl、PostgreSQL、Outbox、NATS与内部Token；超时属于Outcome Unknown，必须以同matchId幂等重试，不能直接写“保存失败”或“已保存”。