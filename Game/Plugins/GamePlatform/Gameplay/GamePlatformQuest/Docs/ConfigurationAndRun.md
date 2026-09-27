# ConfigurationAndRun（配置与运行）

`DefaultGamePlatformQuest.ini`当前配置：MaxActiveQuests=32、MaxRecentEventIds=256、ProgressFlushDelaySeconds=1.0、ClientMaxTrackedQuests=5。这些是开发安全默认，不是经过压力测试的生产最优值。

Backend `go.mod`当前声明 Go 1.25.0 与 `github.com/jackc/pgx/v5 v5.11.0`；Runner 没有 Go 工具链，因此 go.sum 未生成、Go 编译与单元测试未执行。不要手工伪造 go.sum。

PlayerDataService 运行要求 `DATABASE_URL`、`PLAYERDATA_INTERNAL_TOKEN`；可选 `PLAYERDATA_LISTEN_ADDR`，默认 `:8082`。当前 Runner 没有 psql、DATABASE_URL，Migration 和 Repository 集成未执行。

真实 Quest Definition `.uasset`必须由 Unreal Editor 创建。当前仅 `Content/Development/README.md`记录 DA_Quest_FoundationTutorial/Interaction/Combat 目标资产，二进制资产数量为 0。
