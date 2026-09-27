# Troubleshooting（故障排查）

PlayerData 启动失败首先检查 DATABASE_URL、PLAYERDATA_INTERNAL_TOKEN、GATEWAY_CALLER_ID 与 ENTITLEMENT_DEFINITION_FILE。Definition 文件缺失或结构非法会直接 fail-fast。

客户端 Snapshot 加载失败检查 Gateway URL、AccessToken、正式 PlayerAuthenticator 和 Gateway→PlayerData Internal Token。

Dedicated Server 授权结果 OutcomeUnknown 时必须按失败关闭权限或上层恢复策略处理，不能把网络未知当作 Entitled。