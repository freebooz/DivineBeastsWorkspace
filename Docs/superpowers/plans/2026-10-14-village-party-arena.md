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

## 2026-10-14当前补全方案（方案交付，不代表已经实施）

用户本次询问“怎么实现组队，给出实现方案”。本节根据当前源码更新实施顺序；前文早期任务清单与“不构建”的授权记录保留为历史。当前另有用户授权的修复／重编译任务，不将其视作自动授权实施下述新增组队功能。

### 当前能复用的部分

- Go已经有邀请、申请、准备、踢人、转让、离队和解散领域及命令入口；Redis已有条件事务与玩家唯一队伍索引实现。它们需要真实生产依赖并发验证，不能用本轮内存开发后端测试代替。
- 客户端已有Party DTO、认证命令、项目ViewModel及公共组队页面。真实Widget资产存在，包括队伍面板、邀请、申请、目标菜单和准备弹窗；历史Monolith编译／重载记录不等于本轮联机验收。
- 五种竞技模式和不可拆分分队算法存在，但项目模式仍为NotConfigured，正式地图及规则未完成装配；竞技命令明确拒绝未完整接入的观察、取消和逐人确认能力。当前不能宣称组队后可进入竞技场。

### 目标交互与状态流

玩家在Village选中真实玩家 → 目标菜单邀请／申请 → 对方接受／队长批准 → 双方队伍面板同步 → 队长选择模式 → 全员准备 → 整队报名 → 找到对手 → 每个人确认 → 后端分配同一MainArena → 每个人获得自己的准入票据 → 现有应用流程转服 → 权威对战 → 后端确认结果与返回准入 → 回到有效Village／OpenWorld实例。

普通队伍最多5人；单人可以直接匹配，1v1不接受多人队伍。2v2至5v5允许不足人数的小队与其他小队／单人补齐，但原队伍始终作为原子单位分队。三人小队选择2v2必须明确拒绝，不能拆开成员。

后端权威阶段为Editable → ReadyCheck → Queued → CandidateConfirmation → Allocating → TransferCommitted → InMatch → Returning → Editable。每次迁移带Revision、操作身份和服务端截止时间。拒绝／超时发生于迁移提交前时取消本次候选、释放全部预留和名单锁；已经提交转服时读取最新阶段并进入恢复流程，不能假装本地取消成功。

### 分六步补齐

| 顺序 | 所有者与源码范围 | 具体实施 | 可交付验收 |
| --- | --- | --- | --- |
| 1 普通组队闭环 | Backend/party、matchapi、Redis；GamePlatformOnline与DBAClient | 复核既有邀请／申请命令、认证主体、ExpectedRevision、ClientRequestID及唯一队伍索引；补带游标的本人队伍／收件箱观察与完整重同步；目标菜单使用服务器玩家ID。 | 两个真实账号互邀／申请，接受后两端显示一致；并发接受不同邀请只成功一个；离队、满员、超时和越权均有明确错误。 |
| 2 准备与原子报名 | Shared契约、matchapi、Redis事务 | 小队准备绑定同一名单版本与模式；全员准备后由队长报名。将票据创建、队伍锁、每人唯一排队预留、主体幂等记录合并为一次受控事务；取消也原子释放。 | 重试不重复出票；改名单使准备失效；任一步存储失败不留下票据或锁；取消与候选预留竞争只有一个有效终态。 |
| 3 真实匹配与逐人确认 | matchmaking、matchservice/orchestrator、生产组合根 | 启动实际队列消费者，按模式／区域／版本分区；有界选择原子票据组成两队。保存候选名单，每人分别回复；全部接受后再分配。实例和签票步骤持久化、带租约代次，可重试与补偿。 | 五模式准确组满双方；小队不拆分、不重复玩家；一人拒绝或30秒超时不送任何成员入场；进程重启、部分签票失败可以恢复或清理。 |
| 4 竞技内容与权威规则 | DBAArena、GamePlatformArena、真实MainArena内容包 | 将五模式正式规则／版本配置完整；通过Monolith创建并保存真实竞技地图、双方各五个出生点及必要碰撞／导航，完成登记和Cook。接入已批准单局歼灭策略、阶段准备及死亡淘汰，不自动复活。 | 两端取得真实Ready服务器的准入；十个账号证明5v5同场且原小队同侧；死亡不复活、同帧双方全灭和时间到均平局。 |
| 5 事件驱动UI与跨服 | DBAClient公共社交；DBAArena竞技适配；既有Flow/Loading/Session | 接入观察、整队取消、准备和逐人确认事件；填充成员、队长、在线、倒计时和禁用原因。Widget只发命令；票据仅交给现有Flow。失活、换账号、世界退出取消请求并隔离迟到响应。 | 邀请／准备／确认弹窗可以完成真实业务；关闭弹窗不偷偷取消；旅行期间队伍不断开；键鼠导航、焦点恢复、断线重连均正确。 |
| 6 结果、安全返回与交付 | UE权威竞技服务器、Go控制面／结果、跨系统Tests | 服务器提交可信结果；幂等确认比赛终态后解锁队伍。控制面重新选择有效返回实例与安全落点，签发新票据；不复用赛前地址。 | 胜负、失败、取消、专服崩溃及旧实例失效都能退出比赛状态；无幽灵队伍锁、重复结果或旧票据重放；实际三目标构建与Cook通过。 |

接口先写入Shared/Contracts唯一真源，再由锁定生成器更新客户端／Go绑定；路径及字段号遵循现有兼容身份，不在Widget或适配器临时编造接口。新增观察合同至少包含Scope、Cursor、Revision、ServerTime、Deadline及ResyncRequired；业务写入携带ExpectedRevision和ClientRequestID，认证PlayerID由Gateway取得而不是信任请求体。

关键持久对象沿用Party与Ticket，增量加入ReadyCheck、MatchCandidate（名单快照及逐人回复）、MatchOperation（分配／签票／迁移步骤）、PlayerReservation（唯一活跃排队／比赛预留）。队伍可编辑名单与比赛名单必须分开。跨Redis、控制面和结果存储使用已存在的持久步骤／Outbox及消费幂等，不声称跨服务原子事务；Redis生产事务首版沿用已有单实例能力，Cluster不受支持时明确拒绝。

### 实施优先级与验收边界

先交付步骤1的两个账号组队，再接步骤2／3及最小真实竞技地图，首先用两个账号验证1v1完整进出，随后用至少四个真实会话证明2v2整队入场，再覆盖3v3、4v4、5v5。两个客户端只能证明邀请和1v1，不能作为五模式完整联机验收。

沿用已确认参数：邀请60秒、申请120秒、赛前小队准备20秒、找到比赛逐人确认30秒、分配／齐员等待60秒、开战倒计时10秒、单局1200秒、断线宽限60秒。参数归后端／服务器配置；UI只展示真实截止时间，不虚构匹配预计时长。

中文说明、权限／取消／恢复回归与真实资源证据随每步交付。公共组队UI归DBAUIPack_Core，适配归DBAClient，竞技UI／规则归DBAArena；GamePlatform不反向引用项目资源。UI资产变更只通过Monolith，编译、保存、冷重载及焦点审计后再做实际客户端人工核验。
