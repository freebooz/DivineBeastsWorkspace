# ServerAuthorityAndAntiCheat（服务器权威与反作弊）

> 2026-09-30状态校正：本页保留旧方案/历史证据，旧DBAServer具体Quest HTTP/事件适配与后端交付宣称未在当前项目文件清单确认，不能作为现行验收。当前行为以[本轮整改说明](DesignRemediation-2026-09-30.md)、README与真实源码为准；本轮未执行数据库/Outbox/网络联调。

实时 Objective 推进只接受 `FGamePlatformQuestEvent`服务器事实。普通 Client 没有 `ProgressDelta`、`CompleteQuest=true`、GrantReward 或内部 Quest Progress Server RPC。

Event.PlayerRuntimeId 必须与 QuestServer 当前注册的玩家 RuntimeId 一致，否则返回 Unauthorized；DBAServer 适配器只在 Authority（权威服务器）执行。

PlayerData 内部读写接口均要求内部 Bearer Token（内部令牌）、X-Game-Server-Id 和与路径 playerId 一致的 X-Bound-Player-Id。Body 中 game_id/player_id 会被服务端路径值覆盖，不能通过 JSON 冒充其他玩家。当前仓库尚无可复用的 GameServerControl/Session“服务器实例确实绑定该玩家”验证器，因此这里只能认定为最小内部边界；完整 Dedicated Server 绑定身份验证仍为未执行。

后端使用 EventIds、ExpectedRevision、CompletionId、数据库唯一约束和事务 Outbox 防 Replay（重放）与重复完成。没有任何代码直接修改 Gold（金币）或伪造奖励到账。
