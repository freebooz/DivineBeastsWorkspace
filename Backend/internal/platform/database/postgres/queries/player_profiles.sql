-- name: GetPlayerProfile :one
-- 获取玩家长期资料。
SELECT player_id, game_id, display_name, data_version, revision,
       tutorial_completed, default_world_id, created_at, updated_at
FROM player_profiles
WHERE player_id = $1;

-- name: UpdatePlayerDisplayName :one
-- 使用Revision进行乐观并发更新，返回受影响后的完整资料。
UPDATE player_profiles
SET display_name = $2,
    revision = revision + 1,
    updated_at = NOW()
WHERE player_id = $1
  AND revision = $3
RETURNING player_id, game_id, display_name, data_version, revision,
          tutorial_completed, default_world_id, created_at, updated_at;
