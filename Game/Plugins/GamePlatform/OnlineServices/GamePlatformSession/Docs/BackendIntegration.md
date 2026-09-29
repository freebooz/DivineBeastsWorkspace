# D05｜实际后端契约与持久化

## 已有接口与本轮边界

当前公共真源仍为Shared/Contracts/GamePlatform/OpenAPI。Gateway已经新增受Bearer认证的 `POST /v1/divinebeasts/world-entry`，用于当前已认证玩家的常驻世界进入；服务端从认证上下文取得PlayerID/SessionID，并核对selectedCharacterId与expectedCharacterRevision，客户端不能直接声明目标GameServer、Endpoint或玩家主体。

Gateway通过现有GameServerControl能力完成 `allocate-world-transfer`，HTTP与gRPC生产装配均已接入。GameServerControl内部HTTP接口统一增加内部Bearer保护；服务器作用域操作同时核对 `X-Game-Server-Id` 与 `X-Game-Server-Boot-Id`，旧进程不能只凭共享Bearer操作新Boot。TransferTicket已绑定GameSessionId、目标Boot、ProtocolVersion与SessionEpoch，目标Admission Provider通过 `validate-transfer` 再验签并形成可信Binding。Shared OpenAPI已经通过仓库唯一contractcodegen保持同步。

## 实际BackendContractMap

实现位于仓库 `Backend/internal/platform/database/postgresadmission/store.go`；数据库真源为 `Backend/migrations/000003_session_admission.sql`。所有方法接受context.Context，取消/超时直接传入database/sql；连接由上层拥有，Store不自动迁移或关闭它。

- `NewStore(database *sql.DB) (*Store,error)`：拒绝nil数据库。需已完成000003迁移和正确角色授权。
- `Reserve(ctx, request Reservation) (string,error)` → `session_reserve`：可信控制面预留一个位置；OperationID全局唯一且绑定授权主体、实例、启动代次、尝试、材料摘要、协议及TTL。重复返回原状态；换参数报SESSION_IDEMPOTENCY_CONFLICT。TTL单位秒，1～120且不超过授权有效期。不是玩家任意选IP的分配入口。
- `Claim(ctx, connection Connection, attemptID string, proof []byte) (string,error)` → `session_claim`：受认证目标服务器领取。原文至少32字节，Go计算SHA-256，数据库仅比摘要。相同实际连接重试可返回Claimed；另一个ConnectionID、错误实例/Boot/Attempt、过期或撤销均拒绝。
- `Commit(ctx, connection Connection, leaseSeconds int) (int64,error)` → `session_commit`：只有真实连接建立且来源权威租约已停止才能提交；租约1～30秒且不越过授权到期。返回后端Epoch，当前尚无UE租约强制执行/续租，不可用于持续生产游戏连接。
- `Release(ctx, connection Connection, epoch int64) (bool,error)` → `session_release`：只能释放确切实例、Boot、Connection和Epoch。重复释放可幂等返回true；旧来源不能更新目标行。
- `Cancel(ctx, reservationID, authorizationID string) (string,error)` → `session_cancel`：撤销Reserved/Claimed；提交已经获胜则返回Admitted，调用者必须改走查询/Leave。
- `Lookup(ctx, reservationID, authorizationID string) (Snapshot,error)`：只允许同一有效授权决策查询结果。返回State、目标身份、Epoch、到期及权威租约，不返回凭据、摘要或ConnectionID。不确定结果应先查询，不直接重签无限票据。

以上PostgreSQL Admission内核仍作为历史/独立事务能力保留，不能从请求体复制Connection.InstanceID当作已认证主体。当前实际运行链已经由GameServerControl + ServerTransfer + GamePlatformServer HTTP Admission Provider形成唯一UE准入路径；玩家主体来自Gateway认证，服务器主体来自注册实例ID+BootID，SessionEpoch由共享仓储生成并在验票时通过服务端已接受Epoch栅栏防旧。后续若重新启用PostgreSQL Admission事务，必须作为同一权威链的持久化实现替换，而不能与当前Redis/ServerTransfer路径并行产生第二套准入真源。

## 数据所有权与原子边界

四张表均由服务器控制面拥有：session_authorizations保存来自身份/玩家服务的短期核验决策；session_instances保存目标实例及启动、世界、协议、容量、就绪与心跳期限；session_reservations保存单次准入摘要和状态；session_bindings保存每游戏/玩家的最新Epoch与权威租约。第一张不是账号库，也不能由客户端直接写入。生产授权同步和撤销传递尚未接通。

数据库函数使用事务级咨询锁73031001序列化本内核写入，并使用行锁/条件更新保证状态迁移。当前是单数据库串行化实现，没有宣称可横向扩容或通过生产压测。所有其他写入方将来也必须遵守相同事务边界，禁止绕过函数直接改绑定。

容量统计包括未过期Reserved/Claimed及仍有权威租约的绑定。Commit在同一事务中把前者变成后者，没有“减少预留、等下次心跳再计在线”的窗口。到期记录保留审计但不占用容量；物理归档清理未实施。数据库重启保留领取记录，不把旧票据恢复Reserved。

示例状态（均为非敏感示意值）：`Reserved → Claimed(connection由服务器生成) → Admitted(epoch递增) → Released`。错误返回包括SESSION_UNAUTHORIZED、SESSION_TARGET_UNAVAILABLE、SESSION_NO_CAPACITY、SESSION_BUSY、SESSION_REPLAY_OR_EXPIRED、SESSION_SOURCE_STILL_AUTHORITATIVE、SESSION_STALE_OR_EXPIRED。它们尚未映射为公共OpenAPI错误，不能手动当作已发布协议。

## 本次真实证据范围

历史隔离测试调用实际Go Store、pgx驱动和PostgreSQL，包含并发事务及真实数据库容器重启。2026-09-29新增验证覆盖Gateway、GameServerControl、ServerTransfer、内部Bearer/Boot身份、世界进入HTTP适配、服务端SessionEpoch防旧、生产Redis Epoch实现编译、gRPC客户端和生产组合编译。它们仍不是UE真实ClientTravel/Admission双进程成功链；完整E2E仍需执行。
