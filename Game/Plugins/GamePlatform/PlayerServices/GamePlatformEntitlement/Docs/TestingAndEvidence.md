# TestingAndEvidence（测试与证据）

`Build/Validation/VerifyEntitlement.ps1（权益静态验证入口）`覆盖 Client、Backend、HeroAuthorization、QuestReward 四个静态门禁。全工作区 `Test-PluginLayers.ps1`只验证结构和字面依赖。

UE 自动化测试源码当前覆盖共享查询 Has/Any/All/Target 与客户端 AccountGeneration 隔离。Go 测试源码覆盖 DefinitionCatalog、Grant 有效期、Revoke selector 与 Quest Reward 稳定 OperationId。

Runner 当前没有 Go/psql/PostgreSQL/UE5.8 完整运行环境，因此 Go tests、Migration 实跑、数据库并发、Gateway 真登录、Hero/Skin 真流程、Quest Reward 消费、Build/Cook 均未执行。