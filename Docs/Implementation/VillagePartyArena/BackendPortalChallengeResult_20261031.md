# SSLM 固定对手约战后端实施结果（2026-10-31）

状态：**源码、Shared合同、完整Go回归、竞态检测和隔离真实Redis仓储回归已通过；等待独立审查。未部署业务服务，未声明UE联机或人工验收完成。**

本轮读取实际根AGENTS、批准的Portal Design/Plan、现有matchapi/matchmaking/HTTP/gRPC/Redis事务实现。工作分支为`main`；使用逐命令`safe.directory`只读覆盖核对已有修改，未修改Git配置、提交或操作只读.git。保留既有Build/Game、UE资源及其它WIP，不使用HWebCodex。

## 实现行为

- 既有`/v1/matchmaking/commands`增加`challenge`、`challengeReply`、`challengeCancel`。命令只新增`targetPlayerId`和`challengeId`，继续使用可信ActorContext、`expectedRevision`和主体域`clientRequestId`。
- 服务器按目标公开玩家ID读取真实唯一成员索引、Party和队长；1v1允许无Party，2v2—5v5要求两支各N人的完整队伍。发起方必须为真实队长且Party版本匹配；不拆队、不构造虚拟成员。
- 60秒邀请保存在同一`QueueState.Challenges`，与准备/队列/候选/比赛共用唯一`Reservations`索引。邀请预留匹配资格，但不设置任一方`RosterLocked`；目标可改队、离队、转让队长或无队伍时创建队伍。版本/队长/名单变化使旧邀请终结。
- 接受在同一事务复核双方Party版本、队长、名单及全员预留，原子创建两张整队票据并锁两队。快速匹配与约战共用`FormFixedCandidate`构造器；约战直接将指定双方组成30秒逐人确认候选，不运行随机搜索、不额外叠加20秒准备。
- 全员接受后沿用原Candidate持久编排、分配、逐人签票、补偿、入场和安全返回；本轮没有第二套匹配器、分配器、签票器或返回执行器。
- 回执指纹绑定约战命令及认证Session；接受后两位队长的最终确认继续核对发起/接受时Session，不能换认证代次覆盖已同意上下文。其余成员首次最终确认沿用既有Gateway可信会话/角色校验。
- `challenge`对象仅活动邀请时投影；终态、候选及以后阶段省略。顶层`state=challengeInvitation`，`participants`包含双方公开成员；只有targetPlayerId本人可回复、ownerPlayerId本人可取消，权限位位于`challenge`对象。邀请期顶层ticketId/candidateId为空；终态取消可能同样无ticketId。
- Memory与Redis均发布双方成员观察事件。Redis在WATCH内先解析目标成员索引、再读取真实Party，接受与名单变化、并发邀请/取消争用同一写域，不能只在内存形成闭环。

## 变更文件

新增六个后端文件：

- `Backend/internal/modules/matchmaking/challenge.go`
- `Backend/internal/app/matchapi/matchmaking_challenge.go`
- `Backend/internal/app/matchapi/matchmaking_challenge_test.go`
- `Backend/internal/app/gateway/matchmaking_codec_test.go`
- `Backend/internal/transport/httpadapter/matchmaking_challenge_test.go`
- `Backend/internal/transport/grpcadapter/matchmaking_challenge_grpcdeps_test.go`

增量修改：

- `Backend/internal/modules/matchmaking/{queue.go,interaction_contracts.go}`：邀请聚合接入、共用候选构造、期限及投影DTO。
- `Backend/internal/app/matchapi/{matchmaking_lifecycle.go,matchmaking_transactions.go,matchmaking_observation.go,party_commands.go,service.go}`：命令、事务约束、通知与软邀请名单边界。
- `Backend/internal/app/gateway/{matchmaking.go,matchmaking_codec.go}`：严格解码与可信角色/世界上下文复用。
- `Backend/internal/platform/cache/redisstore/{matchmaking_transactions_production.go,matchmaking_transactions_production_test.go,party_transactions_production.go}`：真实双方Party/成员WATCH、授权通知、生产仓储竞争回归。
- `Backend/internal/app/matchservice/runtime_test.go`：同一五模式分配/签票未知响应恢复、入场/结果/返回测试扩展为快速匹配和约战20场景。
- `Shared/Contracts/GamePlatform/OpenAPI/matchmaking-lifecycle.openapi.yaml`：增量1.1.0合同；旧必需字段和路由不变。
- `Backend/DirectoryTree_CN_V1.1.0.md`、`Shared/DirectoryTree_CN_V1.1.0.md`：目录职责登记。

## RED与GREEN证据

原始实质RED命令：在`Backend`执行`go test ./internal/app/matchapi -run '^TestChallengeFixedSoloCandidate$' -count=1`，实际失败为`MATCH_COMMAND_INVALID: 未注册命令`。随后五模式满队、终态、忙碌、幂等、并发测试均先实跑失败于未注册命令，之后才接入约战应用分支。

自查新增的认证边界RED：`TestChallengeFinalReplyRejectsNewLeaderSession`实际失败于`owner新会话覆盖了邀请已同意的上下文`。修正内部来源Challenge和两位队长Session校验后GREEN。证据为`Saved/Validation/VillageArenaPortal20261031/Backend/session-{red,green}.log`。

最终结果：

| 门禁 | 实际命令与结果 |
| --- | --- |
| Shared生成同步 | 工程内Go1.27.2运行`go run ./internal/tools/contractcodegen -workspace-root=<动态绝对工作空间根>`及`-check`，退出0；OperationId/路由/目录未变，因此生成注册表无实际diff。未手改生成物。 |
| 完整后端 | 工程内Go1.27.2：`go test -tags productiondeps,grpcdeps ./... -count=1`，退出0；29个有测试包通过、36个包无测试文件。包括新增HTTP真实往返及gRPC真实适配方法。 |
| 竞态 | 既有`golang:1.23.12`禁网自动删除容器：`go test -race ./internal/app/matchapi ./internal/modules/matchmaking ./internal/app/gateway ./internal/app/matchservice -count=1`，四包通过。 |
| 真实Redis | 临时`redis:7-alpine`无卷隔离容器：`go test -tags productiondeps,grpcdeps ./internal/platform/cache/redisstore -run '^TestRedis(Matchmaking|Challenge)' -count=1 -v`；两项通过。双客户端固定2v2、目标Party解析、持久通知/跨客户端幂等、目标转队长、12并发邀请及接受/取消竞争均实际执行。 |
| 共用运行链 | `TestRuntimeFiveModesAndSafeReturn`的20个快速/约战×人数×故障场景通过；采用真实领域/应用编排与内存隔离存储及明确故障注入，不是实际UE服务器联机。 |
| 差异检查 | `git diff --check -- Backend Shared`无空白错误；仅有本机LF/CRLF转换提示。 |

所有Go编译缓存、模块缓存、临时文件及容器内缓存均位于动态解析的工作空间`Saved`下。原生HTTP测试曾被沙箱拒绝临时回环连接，改在获准的本地socket环境重跑，最终完整回归退出0。未关闭校验或删除缓存。

最终隔离Redis容器：`sslm-challenge-test-71873cfad91e`，ID `3a74e5a84f117631eb5b4102f2307e9143a43c6a57f9609b37551fc85e2fcb05`，仅绑定`127.0.0.1:63131`。脚本finally核对ID对应的容器名称后删除；未接触其它容器、卷或业务DB。运行脚本、元数据和清理回执均保留在下列证据目录。

完整日志：`Saved/Validation/VillageArenaPortal20261031/Backend/{all-tests.log,race-tests.log,redis-tests.log,runtime-tests.log,session-red.log,session-green.log,redis-container.json,redis-cleanup.log,RunChallengeRedis.ps1}`。

## 尚未执行的门禁

- 没有部署/重启五个业务服务或Village/MainArena，没有更改生产Redis或账户/角色数据。
- 没有运行`contractcodegen -official`全量官方Proto/OpenAPI绑定生成；现有运行路径使用同一手工DTO和已生成JSON Envelope，offline注册表生成与check已执行。
- PostgreSQL/Redis等其它既有显式环境集成测试，除本轮指定隔离Redis回归外，按原测试环境条件跳过；完整Go回归通过不等于所有外部系统已运行验收。
- 尚无两真实UE会话的1v1门→邀请→逐人确认→准入→结算→返回证据；2v2—5v5也没有4/6/8/10真实会话人工验收。构建/Cook、运行战斗版本一致性和人工验收由主任务统一执行并分别报告。
- 不新增“原场再战”特殊准入；第一阶段仍按批准设计返回集结区重新邀请。
