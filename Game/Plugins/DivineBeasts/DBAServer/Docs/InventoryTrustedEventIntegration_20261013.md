# SSLM可信背包事件：生产链路、约束与待验收项（2026-10-13）

本文件属于`DBAServer`（项目专用服务器组合根）文档，描述玩家在Village（新手村）和OpenWorld（常驻世界）采集、领奖、使用物品时的后端联动，不新增服务器角色或绕过既有准入。

## 已写入的三层实现

平台背包后端`inventory`（背包权威领域）：`ConfirmedInventoryEvent`（游戏服已确认事件）携带GameID（游戏ID）、GameServerID（实例）、ServerBootID（启动代次）、SessionEpoch（服务器玩家世代）、ConnectionID（真实连接号）、SourceEventID（稳定玩法事件ID）及物品与数量。`TrustedEventProcessor`（可信事件处理器）先验证当前会话租约，再记录不可变事件，通过同一OperationId（幂等操作编号）执行GrantTrusted/ConsumeTrusted（授予/消费）。服务端读取最新库存Revision后进行最多3次有界并发冲突重试；只有Revision可变，物品、数量、连接、事件来源不可修改。历史重复提交通过仓储忽略并发修订号的可信事件意图指纹返回原结果。

数据库层新增`000011_player_inventory_trusted_events.sql`（可信事件日志），`postgres.InventoryAuthorityRepository`（生产会话+事件仓储）查询000003中的session_bindings/session_instances/session_reservations/session_authorizations（准入绑定、实例、预留及身份授权），并在事务中用FOR SHARE锁定租约。Grant和Consume（授予/消耗）最终事务再检查相同租约，以避免事件校验后玩家已重登/换服。库存修改、幂等操作、Inventory.Granted/Consumed Outbox（可靠集成事件）仍共用单一数据库提交。

传输入口`POST /internal/v1/playerdata/inventory/trusted-events`（仅游戏服的受控内网HTTP接口）使用**独立强Bearer令牌**、与事件完全匹配的X-Game-Server-Id/X-Game-Server-Boot-Id（实例与Boot标头）、严格JSON和请求大小限制，再进入会话数据库核验。普通PlayerData Handler与公网Gateway均没有此路由。生产HTTP模式及productiondeps+grpcdeps（生产gRPC模式）按部署配置分别装配HTTP内部事件入口，不将普通玩家HTTP库存接口额外暴露给gRPC模式。

`DBAServer/Source/DBAServer/Public/Inventory/DivineBeastsTrustedInventoryEventSubsystem.h`（SSLM专用服务器事件出口）和对应Private C++实现只在Dedicated Server创建，读取平台服务器当前`FGamePlatformServerVerifiedAdmission`（已验证玩家准入投影），核对Boot、连接号、世代、有效期；调用方可省略库存版本（协议0表示由PlayerData在授权后自行获取权威Revision并执行乐观锁），或提供Dedicated Server实际读取的Revision，但绝不可接受客户端自报版本。`SubmitWorldPickup`（采集）、`SubmitQuestReward`（任务奖励）和`SubmitItemUse`（使用）均需稳定世界事件GUID、真实服务器玩法判定；它们不是可由玩家提交的RPC/蓝图授权入口。响应显式区分首次成功、重复成功及结果未知；超时不得用新事件号重试。服务器退出取消自有HTTP任务和令牌副本。

## 生产配置及权限边界

- `INVENTORY_TRUSTED_EVENTS_TOKEN`：PlayerData与Dedicated Server部署的独立内部强随机令牌，至少32字节，严禁提交代码/客户端包，不得复用GameServerControl令牌。
- `INVENTORY_GAME_ID`：当前后端游戏编号。SSLM按现有后端账号域使用`divine-beasts`，不是UE技术工程目录名称`DivineBeastsArena`；服务启动时校验策略文件gameId。
- `INVENTORY_POLICY_FILE`：后端只读受审物品策略文件，可基于`Backend/config/inventory_item_policies_v1.json`（首批8种物品）复制到受管部署路径。
- `PLAYERDATA_BASE_URL`：Dedicated Server连接的PlayerData内部HTTP根地址；gRPC业务模式仍须为专用事件提供受控HTTP端点。
- `GAME_SERVER_ID`、`GAME_SERVER_BOOT_ID`：服务器控制面分配的实际实例及进程启动代次，缺少则可信事件发送失败关闭。

在Kubernetes/Agones（容器编排与游戏服调度）部署中仍应配合mTLS（双向TLS）、NetworkPolicy（网络策略）、Secret（受管凭据）、独立数据库角色和最小权限；单凭HTTP头及游戏服共享令牌不等于硬件级防冒充。

## 会话准入数据闭环审查（真实源码定位）

已核对UE`GamePlatformHttpAdmissionProvider.cpp`（平台服务器准入HTTP提供者）：当前实际请求路径是`/internal/v1/gameservers/validate-transfer`（游戏服迁移票据校验），响应被映射为`FGamePlatformServerVerifiedAdmission`（客户端/服务器的准入投影）；该函数本身并不写入`session_bindings`（玩家会话绑定表）。Go`Backend/migrations/000003_session_admission.sql`（会话准入数据库脚本）则以`session_commit`写入`session_bindings`，并要求当前绑定`authority_until`未过期；`Backend/internal/platform/database/postgresadmission/store.go`（准入存储）声明单次提交租约不超过30秒。此前未发现自动续租器；目前已补入专用服务器定时续租源码，但它与上述UE票据验证路径一致的**首次持久绑定写入**仍未得到实际运行证据。因此PG可信库存授权在缺少真实绑定时将安全拒绝，**不能标记联机发奖已可用**。

下一轮必须在现有控制面明确补齐会话提交/续租/退出的权威来源，再让库存与世界准入共用同一连接、Boot、SessionEpoch。避免仅向`session_bindings`写测试数据或延长租约常量以伪造有效绑定，必须用真实登录和进入Village的专服数据核验。

## 已追加的受控会话续租实现（待统一验收）


2026-10-13会话续租增量：新增 `000012_player_inventory_session_lease_renewal.sql`（已提交真实准入绑定的30秒续租数据库函数），要求在线身份会话未撤销、账号未禁用、服务器保持Ready/健康、当前连接与Boot及SessionEpoch一致。它不创建绑定、不绕过原始 `session_reserve→session_claim→session_commit`（预留→领取→提交）链路，也不复活已经过期的租约。PlayerData生产适配器 `inventory_session_lease_repository.go`（续租仓储）及内部 `POST /internal/v1/playerdata/inventory/session-renew`（服务器会话续租接口）与已有独立Bearer保护共用；公共Gateway不注册。项目Dedicated Server（专用服务器）订阅平台真实准入变更并每10秒尝试续租当前玩家，退出/撤销时精确取消本实例续租。由于现有 `validate-transfer`（迁移票据校验）尚未被核实与 `session_commit` 实际联通，续租只改善**已经正确绑定的会话**，不代表首个进入新手村的玩家已经可以领取物品。上述迁移、Go生产适配和UE代码均待正式编译及实际PostgreSQL验证，严禁将源码存在记录为运行完成。

## 尚未交付的功能和风险

1. 游戏服原有世界交互与任务结算尚未产生正式唯一SourceEventID并调用本C++出口；采集范围、掉落资格、冷却、采集者所有权、同一掉落实例只被消费一次，仍由权威玩法系统处理。C++事件出口存在**不代表**游戏内已经可以拾取或领奖。
2. 使用物品扣除后如何执行恢复生命、Buff/DeBuff（增益/减益）和战斗GAS（Gameplay Ability System，玩法能力系统）效果，必须有下游事件消费者和效果事件去重；不得因同一Inventory.Consumed重放多次治疗。
3. 当前没有在实际PostgreSQL服务应用000011及前序迁移的成功证据，没有真实SessionEpoch跨连接联机数据与并发压测，也没有从游戏新手村输入到可靠入包/消费的两客户端结果。
4. 物品显示蓝图`WBP_DBA_UI_Inventory`（背包页面，172控件）已在前轮真实交付；本轮8张物品Icon Texture2D（图标纹理）及对应DataAsset（显示定义）受Monolith连接状态限制，仍未通过引擎制作。详见`DBAUIPack_Core/Docs/InventoryVisualAssets_20261013.json`（物品图标资产制作清单）。
5. 本轮新代码必须完成Go默认与productiondeps/grpcdeps变体测试、UE Editor和DedicatedServer模块编译、数据库迁移及一站式人工测试后才可标记“完成”。


## 2026-10-13 补强：可信事件响应与消息信封 V2

本次新增的 `inventoryOutboxEvent`（可信库存事务发件箱信封）采用 `schemaVersion=2`（消息版本2），为 `Inventory.Granted`（授予事实）和 `Inventory.Consumed`（扣除事实）增加 `gameId`（游戏标识）、`sessionEpoch`（准入会话世代）、`connectionId`（真实连接号）；消耗事件另包含 `itemInstanceId`（实际消耗的物品实例标识），`remainingQuantity`（剩余数量）即使是0也显式输出。事件仍保留稳定 `operationId`（幂等操作编号）、`sourceEventId`（玩法来源事件）以及服务器Boot（启动代次）。这些内容仅用于下游可靠消费的身份核对，**不是新颁发的服务器权限或一次性通行证**。

`playerdata_server_trusted_events.go`（可信事件HTTP处理器）新增严格POST方法、RFC媒体类型校验、最大8KiB请求体限制，复用 `DecodeOnlineJSON`（严格JSON解码）拒绝重复键、未知字段和null必填；独立Bearer验证始终先于解析正文。项目专服 `DivineBeastsTrustedInventoryEventSubsystem.cpp`（项目事件发送子系统）设置拒绝HTTP重定向，环境缺少该引擎能力时拒绝发请求；通过FHttpRequestStreamDelegateV2（HTTP流式响应处理委托）接收，硬限制256KiB，并核验回执操作号格式、数量和版本。玩家界面关闭/切换时卸载物品图标画刷和异步句柄，不显示前一账号数据。

新测试源文件：`Backend/internal/platform/database/postgres/inventory_trusted_outbox_test.go`（Outbox V2字段契约测试）、`Backend/internal/transport/httpadapter/playerdata_trusted_events_hardening_test.go`（HTTP拒绝非POST及伪造媒体类型测试）、`Backend/internal/app/gateway/inventory_trust_error_test.go`（可信事件错误码映射测试）。这些**尚需统一Go测试与真实UE编译确认**；目前不能据此声称消息被NATS实际发布、物品效果已运行或两客户端联机通过。

## 统一验收建议

正式门禁按顺序覆盖：旧Boot/旧Epoch/旧Connection拒绝；普通Gateway不能调用可信事件；同来源奖励重复投递数量仅增加一次；原事件改变物品或数量拒绝；背包满不发部分奖励；两个不同真实事件在玩家并发移动背包时能安全串行恢复；一次使用至零数量清除快捷栏；服务重启与Outbox重复发布不会重复游戏效果；登录、角色创建/选择、新手村、拾取、背包整理、注销重登、两客户端及资源Cook全链路通过。

Cwebcodex写入代码或Monolith保存资产不能替代上述运行证据。
