# GamePlatformInventory（游戏平台背包插件）

正式稳定路径：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformInventory`。该插件属于 GamePlatform（平台层）PlayerServices（玩家服务适配），UE 侧只保留 `GamePlatformInventoryClient（背包客户端模块）`，Type=`ClientOnly（仅客户端）`；长期背包权威位于 `PlayerDataService（玩家数据服务） + PostgreSQL（关系数据库）`。不创建第二套 UE Server 背包真源。

当前已实现的客户端能力包括 Snapshot/Revision（快照/修订号）、Container/Slot（容器/槽位）、ItemInstance（物品实例）、Move/Swap、Split/Merge、Quickbar、Pending Operation、OperationId、ExpectedRevision、RevisionConflict 全量对账、AccountGeneration 与 SnapshotRequestGeneration 迟到响应隔离、派生排序/索引缓存和中立 ViewModel。默认 Transport 已改为复用 `GamePlatformOnlineClient（平台在线客户端）`的认证请求通道，不保存 AccessToken，也不自行实现 Token 刷新。

当前已实现的后端 HTTP 链为：`Gateway /v1/inventory... → PlayerData internal HTTP → Backend/internal/modules/inventory → PostgreSQL`。实际源码位于 `Backend/internal/modules/inventory/inventory.go`、`Backend/internal/platform/database/postgres/inventory_repository.go`，数据库真源为 `Backend/migrations/000008_player_inventory.sql`。OperationId 同键同内容支持持久幂等重放，同键异内容拒绝；背包状态、聚合 Revision 与 Operation 结果在同一 PostgreSQL 事务提交。

当前尚未实现可信 Dedicated Server 的 Grant/Consume（授予/消耗）、Quest Reward（任务奖励）、External Pickup（外部拾取）以及对应 Outbox 事件；这些能力不得由普通客户端 API 代替。生产 gRPC 的 `player-data-service.proto` 已增加 Inventory RPC 协议源，但当前 Runner 缺少 Go/Protobuf/OpenAPI 代码生成工具，因此生成绑定和 gRPC Inventory Adapter 仍待正式 Codegen。Equipment（装备）、Entitlement（权益）、Economy（经济钱包）继续保持独立领域。

验证状态以源码和实际工具执行为准：当前 Runner 无 `go/protoc/protoc-gen-go/protoc-gen-go-grpc/oapi-codegen`，因此 Go Test、Proto/OpenAPI Codegen 和生成物新鲜度尚未执行；UE 模块构建需在当前并行 UBT 构建释放全局互斥后再次验证。详见 `Docs/审查整改执行计划.md`。



## 2026-10-09 本轮公开合同与整改

默认LocalPlayer初始化订阅所属GI Online认证事件，只消费脱敏AccountId和安全请求通道，不保存认证票据。`RefreshSnapshot`发布Loading前快照Transport和账号/请求代次，监听器内ResetAccount后整份旧请求拒绝启动；Reset事件的再次Reset是幂等调用，广播内Configure拒绝。

写请求最多一个未决OperationId。Move目标槽位范围由权威容器Capacity决定；Split数量必须大于0且小于源数量；Merge实例必须不同且不超过权威MaxStackSize；Quickbar槽位范围为0到11。返回Guid只表示幂等命令已受理，结果未知时保留原OperationId并先查询，不新建操作重复写入。只读快照/指针/缓存引用不能保存跨账号或状态事件。

回归入口：`GamePlatform.Inventory.Client.ResetDuringState`验证Loading广播内重置不启动旧传输；既有Client用例覆盖未决操作、版本冲突与账号隔离。

以上Automation源码已维护，但本轮分工阶段没有执行UE构建/Automation。实际Editor、Client、Server、Cook和联机证据由根整改账本统一记录；静态源码门禁与原生CMake结果不替代这些验收。中文审核覆盖本轮修改的公开字段、命令范围、线程、异步终态、账号/资源所有权及Build责任；未据此宣称全部历史源码已完成中文审核。

### 2026-10-09 实例退出与同步回调补充

LocalPlayer服务在Deinitialize开始即关闭实例作用域并推进InstanceGeneration/AccountGeneration。Configure的Reset通知同步关闭服务后，外层账号配置不得恢复Transport或发请求；公开刷新/命令及迟到回调同样拒绝已关闭实例。Cancel与Begin调用以局部Transport保活，避免通知释放成员后旧栈继续访问已析构对象。Initialize仅建立新实例代次，不将旧Completion当作新账号结果。

新增CloseDuringConfigure回归使用本领域实际Subsystem和只在Tests存在的手控Transport，验证退出后不发请求。UE自动化尚未执行，运行结果由统一UE验证补证。

Pending写入与操作查询在外部Begin前登记独立终态门闩，同步OutcomeUnknown/Error不会被返回栈重写为Mutating。查询捕获原OperationId、账号及局部Transport；重复/旧结果不得消费下一次查询资格。快照读取也必须同时匹配请求代次和在飞门闩。

### 广播监听器接管恢复资格

Mutation与操作查询完成后仍把捕获的OperationRequestGeneration传入内部处理器，不能只依靠账号和相同OperationId判断旧栈有效。OperationNotFound通知监听器启动查询B后，旧查询A不得重发Mutation；查询B即使同步完成，代次变化仍表示恢复资格已接管。

RevisionConflict通知后复核操作请求代次、在飞标记、SnapshotRequestGeneration及对账资格。监听器已受理或完成Snapshot对账时旧Mutation栈停止。RefreshSnapshot自身拥有启动失败终态，外层不能把“未受理／已接管”一概标成BackendUnavailable。

QueryListenerTakeover与ConflictListenerTakeover使用实际OnChanged通知和测试Transport入口计数，覆盖无额外重发、无伪后端错误、新查询/快照仍可正常完成。本分工未执行UE回归。
