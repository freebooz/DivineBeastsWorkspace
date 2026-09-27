# Troubleshooting（故障排查）

RegisterPlayer 返回 DefinitionMissing/DefinitionVersionMismatch：检查非 Completed 历史任务的 Definition 是否已加载且版本兼容；已完成任务允许保留历史版本，不应倒退。

事件不推进：检查 Event.PlayerId/PlayerRuntimeId、EventType、SemanticTags、RegionId 和当前 EventIndex；Combat 必须是当前玩家作为 Source 的 Death 事实，Interaction 必须是 Committed 类终态。

出现 RevisionConflict：不要再次 +1。QuestServer 会尝试从 PlayerData 重新加载 Snapshot Reconcile；若仍失败，保留 Pending 状态并返回错误。

任务完成但消息未消费：检查 player_quest_progress 是否 Completed、player_quest_completion、gameplatform_outbox 是否同事务存在，再检查 Dispatcher lease/last_error/published_at。当前无具体 NATS Publisher 时 published_at 不应被假定已更新。
