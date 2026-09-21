-- name: GetMatchResult :one
-- 按MatchID读取权威比赛结果，用于幂等检查。
SELECT match_id, result_id, arena_mode_id, game_server_id, winning_team_id,
       started_at, ended_at, payload, created_at
FROM match_results
WHERE match_id = $1;

-- name: InsertMatchResult :exec
-- 插入MainArena Dedicated Server提交的权威比赛结果；MatchID主键保证幂等边界。
INSERT INTO match_results (
    match_id, result_id, arena_mode_id, game_server_id, winning_team_id,
    started_at, ended_at, payload
) VALUES ($1, $2, $3, $4, $5, $6, $7, $8);
