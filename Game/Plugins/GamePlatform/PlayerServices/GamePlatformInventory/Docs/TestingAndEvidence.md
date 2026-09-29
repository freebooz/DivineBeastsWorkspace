# TestingAndEvidence（测试与证据）

本文件只记录当前可复核证据，不把旧任务或未执行命令冒充为本轮验证。审查整改计划见 `审查整改执行计划.md`。

UE Automation Test（自动化测试）源码现覆盖：初始 Snapshot、同账号 Snapshot 单飞、Mutation Revision、RevisionConflict→专用 Reconcile、AccountGeneration 迟到响应隔离、OperationId 结果未知恢复、派生排序/索引缓存、重复 ItemInstanceId 快照拒绝，以及确定性业务错误清理 Pending 并保留 LastError。默认 Transport 通过 GamePlatformOnlineClient 取消本插件自己的请求，不再直接持有 IHttpRequest 或 AccessToken。

Go 侧新增 `Backend/internal/modules/inventory/inventory_test.go`，覆盖内存仓储 OperationId 同键同内容幂等、同键异内容拒绝、Revision 单调递增、Split/Merge 和 Quickbar 引用重定向；新增 `Backend/internal/app/gateway/inventory_test.go`，覆盖公网背包必须使用认证主体、超过 2^53 的 Revision 仍以字符串无损返回，以及 Move 请求字符串 Revision 的精确解析。生产 PostgreSQL 仓储与 `000008_player_inventory.sql`已落盘，但当前 Runner 没有 Go/psql/PostgreSQL 工具链，因此这些测试和真实事务集成尚未执行。

本轮曾尝试 UBT 增量构建：第一次因相对 `.uproject` 参数未解析而未进入编译；使用绝对路径重试时被另一会话正在运行的 UnrealBuildTool 全局互斥阻止，仍未进入本模块编译。这两次失败都不是源码编译结论。必须在互斥释放后重新执行 `GamePlatformInventoryClient` 构建和 UE Automation Test。

已实际通过的非编译验证包括：`GamePlatformInventory.uplugin` JSON 解析、`inventory.openapi.yaml` YAML 解析、OpenAPI/Gateway/UE 七条路由一致性、后端稳定错误码到 UE 映射覆盖、OpenAPI 本地 `$ref` 完整性、公网 Request 无 playerId、`git diff --check`、项目头文件存在性门禁和三层继承边界门禁。Go/C++ 源码还执行了括号/块结构完整性预检，但这些预检不替代真正编译。

当前 Runner 明确缺少 `go`、`protoc`、`protoc-gen-go`、`protoc-gen-go-grpc`、`oapi-codegen`，因此 Go Test、内部 Proto 生成、Shared OpenAPI Codegen 和 Generated 新鲜度尚未验证。Grant/Consume、Quest Reward、External Pickup 尚未实现，也没有相关测试通过证据。
