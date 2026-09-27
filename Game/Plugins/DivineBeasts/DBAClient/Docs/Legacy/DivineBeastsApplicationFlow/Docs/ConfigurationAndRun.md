# ConfigurationAndRun（配置与运行）

静态综合验证：

powershell -NoProfile -ExecutionPolicy Bypass -File Build/Validation/VerifyDivineBeastsApplicationFlow.ps1

专项验证：

Tests/Integration/ApplicationFlow/TestDivineBeastsApplicationFlow.ps1
Tests/Integration/ApplicationFlow/TestCharacterEntry.ps1
Tests/Integration/ApplicationFlow/TestWorldEntry.ps1
Tests/Integration/ApplicationFlow/TestApplicationReconnect.ps1

契约验证和干净再生成：

Build/Contracts/Validate-DivineBeastsContracts.ps1
Build/Contracts/Test-DivineBeastsContractsClean.ps1

后端启用需要 DIVINEBEASTS_APPLICATION_BACKEND_ENABLED=1，并按现有Gateway/PlayerData/GameServerControl配置数据库URL、内部服务Token、世界Ticket Secret和Allocator模式。

开发Allocator支持 `development-static（静态开发分配）`。`agones（Agones分配）` 已提供 Kubernetes `GameServerAllocation` API Adapter，需要 `DIVINEBEASTS_AGONES_KUBERNETES_API_URL`（可由KUBERNETES_SERVICE_HOST/PORT推导）、`DIVINEBEASTS_AGONES_NAMESPACE`、Bearer Token或Token File、可选CA文件、Port Name和Timeout。GameServer应提供 `divinebeasts.dev/experience-id`、`region-id`、`map-id`、可选`world-id`标签。

真实UE Editor/Client/Server Build、Client Cook、Go test/vet/race、PostgreSQL/Redis/Agones集群联调未执行时不得写通过。
