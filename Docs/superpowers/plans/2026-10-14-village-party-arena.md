# SSLM新手村组队与竞技场闭环实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. 用户明确要求直接执行、不再问询，采用Native，不派发实现子任务。

**Goal:** 补齐普通小队邀请／申请、五模式匹配与真实准入、单局歼灭及安全返回，不引入长期战队或单排轮斗。

**Architecture:** GamePlatformOnline提供中立组队契约，公共项目UI只消费只读快照和提交命令；DBAArena组合匹配、规则与唯一Flow。Go拥有业务名单和队列，UE服务器拥有战斗与淘汰，资源仅由真实引擎工具制作。

**Tech Stack:** 锁定UE5.8、现有Go1.23模块、Redis、PostgreSQL事务发件箱、现有Gateway／HTTP／gRPC及Monolith MCP。

**Spec:** `Docs/superpowers/specs/2026-10-14-village-party-arena-design.md`。

## Global Constraints

- 用户2026-10-14明确“请执行，不要问询”，视为采用规格建议细则并豁免后续阶段问询，安全与不可逆外部操作仍不扩大授权。
- 直接当前main，不创建工作树，不启动UE／Go构建、LiveCoding、Cook或Stage；编译型测试只写入并记录待统一执行，禁止伪称TDD红绿已运行。
- 后续明确授权例外：用户2026-10-14允许“仅构建正式工程Editor目标所需模块，然后重启编辑器”。本例外只恢复Editor必要依赖的真实增量编译／链接及启动；Client／Server、Go构建、Cook／Stage仍不在本轮授权内，既有其他任务的构建不由本任务中断或接管。
- 1v1至5v5，普通队伍不可拆分，单人无需先创建队伍；不复制身份、资产加载、竞技框架或流程执行器。
- 邀请60秒、申请120秒、组队准备20秒、逐人确认30秒、分配等待60秒、入场准备60秒、开战10秒、单局1200秒、断线宽限60秒。
- 正式无复活，双方同帧全灭／时间到平局；不做选禁、重复英雄允许；正式数据不足时维持不可报名，开发数据不得冒充生产。
- 全部修改代码和公开合同附中文职责、所有权、权限、失败及生命周期说明；保留其他任务未提交修改与历史冲突原文。

## Review Focus

- 同一目标并发接受不同队伍邀请时只能归属一队；域规则测试归任务1，跨仓储唯一索引归任务2。
- 相同幂等键由不同玩家使用不能读到他人票据；任务2增加主体域和异参冲突回归。
- 取消、候选提交、签票失败及工作进程重启不能留下名单锁；任务3覆盖条件迁移和补偿恢复。
- 同帧双杀、无击杀者死亡、死亡者重连不能触发先到先赢或满血复活；任务4覆盖权威淘汰事件。
- 关闭竞技、多World和换账号后旧UI回调必须失效；任务5覆盖作用域、代次与委托释放。

---

### Task 1: 普通队伍领域命令与回归

**Files:** 修改`Backend/internal/modules/party/party.go`；新增同目录`membership_requests.go`、`membership_requests_test.go`，更新既有`party_test.go`。

**Interfaces:** 保留New／AddMember／RemoveMember／SetReady；新增`Invite(actorPlayerID, requestID, targetPlayerID string, now time.Time) error`、`RequestJoin(requestID, playerID string, now time.Time) error`、`ResolveMembershipRequest(actorPlayerID, requestID string, accept bool, now time.Time) error`、`TransferLeadership(actorPlayerID, targetPlayerID string) error`、`Leave(playerID string) error`。领域不调用数据库或HTTP，跨队唯一归属由任务2事务保证。

- [ ] 先写邀请接受／拒绝／过期、申请批准权限、满员、锁定、重复终态和队长转让／离队测试；预期错误不改变Revision／成员。
- [ ] 增量实现有界请求、服务端时间、单调加入序号、准备失效和错误码；兼容已有Party序列化字段。
- [ ] 不构建约束下执行gofmt及差异检查；`go test ./internal/modules/party`延后，不标测试通过。

### Task 2: 事务仓储、主体幂等与正式接口

**Files:** 修改`Backend/internal/app/matchapi/service.go`、`Backend/internal/platform/cache/redisstore/repositories_production.go`；扩展`Backend/internal/transport/{httpadapter,grpcadapter,grpcclient}`现有Match入口及`Backend/internal/app/gateway/api.go`；契约真源在`Shared/Contracts/GamePlatform/OpenAPI/{gateway,party,matchmaking}.openapi.yaml`、内部RPC在`Backend/internal/contracts/proto/match-service.proto`。

**Interfaces:** 普通命令带认证PlayerID、ClientRequestID、ExpectedRevision；持久层提供条件更新和唯一成员索引。队伍与票据写入同一原子事务，不能仅改Memory实现；所有新公开字段均先进入真源。

- [ ] 先写不同主体同键、同主体异参、非队长／非成员、非法JSON、并发邀请接受及事务失败回归。
- [ ] 实现Redis条件更新、玩家唯一队伍／队列索引、可恢复邀请收件箱与游标；增加Gateway授权后路由及真实HTTP／gRPC适配。
- [ ] 锁定生成器可用时只运行代码生成，不运行构建；不可用则保留真源与明确生成阻塞，不手改产物。
- [ ] `go test ./internal/app/matchapi ./internal/transport/...`及productiondeps／grpcdeps验证统一延后。

### Task 3: 真实队列与逐人确认及迁移编排

**Files:** 扩展`Backend/internal/modules/matchmaking`、`Backend/internal/app/matchservice/orchestrator.go`及两生产组合根；持久步骤复用`Backend/internal/platform/outbox`与PostgreSQL适配，不新建服务入口。

**Interfaces:** 模式来自现有ArenaModeByID，候选由不可拆分Ticket组成；确认状态带Revision和截止时间，调用现有ArenaControl的AllocateMainArena／IssuePlayerTransfer端口。

- [ ] 先写五模式原子补齐、超员、重复玩家、取消竞态、全员接受／超时、签票部分失败和进程恢复用例。
- [ ] 实现有界候选消费、租约栅栏、准备和逐人确认、持久操作步骤、重试与撤销；生产组合根实际启动消费者。
- [ ] 连接现有控制面与Session世界准入、结果及返回；无真实地图／Ready实例保持明确失败，禁止假Assignment。
- [ ] 编译型测试、真实Redis／PostgreSQL及双客户端联调统一延后。

### Task 4: 可配置权威淘汰与重连

**Files:** 修改`Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformMobaCore/Public/Arena/GamePlatformArena{Types,Policies}.h`、`GamePlatformArena/{Public,Private}/Framework/GamePlatformArenaGameMode.*`及模块Private/Tests；项目配置及服务器适配归`Game/Plugins/DivineBeasts/DBAArena/Source/`。

**Interfaces:** 复用现有可信Death事件、MatchId／Pawn／AvatarGeneration门禁和Score／WinCondition契约；公开只读淘汰状态，新增功能默认不改变既有积分测试模式。

- [ ] 先写无复活、环境死亡、重复死亡、同帧双方全灭、断线后死亡／重连、终局迟到事件测试。
- [ ] 按显式规则启用淘汰路径，帧末合并判胜，禁止已淘汰者通过重连或出生再次Active；保持旧积分模式兼容。
- [ ] 五模式配置记录明确规则，真实地图／内容未到位不改ProductionReady；不对测试地图或开发技能放宽生产校验。
- [ ] UE测试只写入；静态依赖门禁可执行，不调用UBT。

### Task 5: 组队与竞技客户端命令和事件适配

**Files:** 扩展`GamePlatformOnline/Source/{GamePlatformOnline,GamePlatformOnlineClient}`、`DBAClient/Source/DivineBeastsUIClient`现有Social合同／ViewModel／Screen；竞技适配归`DBAArena/Source/DivineBeastsArenaClient`。

**Interfaces:** 复用在线请求句柄、认证、Cancel和GameInstance作用域；Social只读投影继续使用SourceScopeId／Revision；竞技动作以公开可撤销提供者注入，不反向依赖竞技。

- [ ] 先写快照倒退／换账号、命令权限、忙碌与失败、旧回调、取消和跨World清理测试。
- [ ] 实现命令、观察订阅、结构化错误与屏幕命名控件合同，事件驱动更新，输入与光标沿用现有仲裁。
- [ ] 匹配成功仅交给既有Flow／Loading／Session，不由Widget直接切图；敏感票据不进入视图状态或日志。
- [ ] 执行静态边界检查，UE测试延后。

### Task 6: Monolith真实资产与地图

**Files:** 公共UI在`DBAUIPack_Core/Content/UI/Social`，竞技UI在`DBAArena/Content/UI`，地图及定义在真正创建后登记的`DBAWorldPack_MainArena`；中文制作证据归所属插件Docs。

**Interfaces:** 使用任务5已实际可加载的父类与命名控件；复用现有玉石Theme和HUD部件，不新建播放器。

- [ ] 核对正式工程、锁定UE、Monolith状态／版本／动作目录；未运行时尝试无编译启动，禁止自动重编或LiveCoding。
- [ ] 引擎可用才创建／修改、编译Widget、保存、重开回读、登记Manifest；没有新父类DLL时保留明确阻塞，不伪造资产。
- [ ] 创建双方五个出生点及准备门禁、碰撞／导航、世界／体验定义；登记真实包和扫描，不建立空插件。
- [ ] 颜色源图／RHI、Cook／Stage与人工视觉核验统一延后，不调用错误Gamma的widget截图路径。

### Task 7: 集成复核与交付记录

**Files:** 本计划、`Docs/Implementation/VillagePartyArena/ExecutionProgress.md`、受影响README、总体目录，以及Backend／Shared细化目录。

- [ ] 回读全部修改、执行不构建的格式／契约／依赖检查并记录退出码；所有未运行测试明确列出。
- [ ] 对整体源码进行一次独立审查，修复重要问题；当前不构建，因此不得标记功能全部验收完成或发布。
- [ ] 更新逐任务实装／待验证／阻塞状态、中文注释范围、剩余资源与生产闭环，保持无关未提交修改。
