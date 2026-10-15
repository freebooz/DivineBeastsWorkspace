# SSLM组队与竞技事件链路

公共队伍合同与GameInstance单订阅归GamePlatformOnline，公共ViewModel、页面绑定和通知路由归DBAClient/DivineBeastsUIClient，真实公共资产归DBAUIPack_Core。竞技模式、匹配适配和局内页面归DBAArena，向下借用MobaCommon与GamePlatform；公共流程通过中立扩展接收竞技就绪事实，不反向引用竞技模块。未新增第二个网络或旅行执行器。

## 队伍与报名

无队伍玩家可直接单排，也可发送首次邀请。后端同一事务创建队伍、邀请、唯一成员索引、收件箱、幂等回执与事件。邀请60秒、申请120秒。队长可管理成员、移交、解散，成员可离队；报名锁定期间不能变更名单。本人邀请到达与本轮准备开始由真实观察通知弹出页面，失活或认证换代解除旧订阅。

队长选择1v1至5v5模式，整队进行20秒准备；单人无需先建队。队伍人数不能超过该模式单队容量，1v1拒绝多人队伍。全部准备后，名单锁、票据、玩家唯一预留和回执同一事务提交。队列按模式/区域分区，以不可拆分票据填满双方精确人数；候选形成后每位玩家须在30秒内亲自确认，队长不能代确认。确认时Gateway复核本人已选角色及真正来源世界。

## 观察与客户端所有权

公共队伍和竞技各只有一条GameInstance认证作用域观察。初始完整快照后续接服务器通知长等待，游标丢失完整重同步。Widget只投影状态、提交意图；无业务Tick或常驻计时查询。倒计时使用CommonNumericTextBlock的引擎端点动画；结束、拒绝和重同步仍以服务端事实为准。

真实竞技适配复用Online认证、同源传输、预算及取消。命令解析Gateway的clientRequestId/snapshot信封并核对原请求身份；本地取消等待不等于远端取消。确定拒绝清除本地意图，响应未知则重新建立授权快照，不盲目重复报名。取消重试使用原版本/请求键，确定冲突后才允许新逻辑重试。页面关闭不停止共享订阅或释放远端名单。

LocalPlayer组合拥有页面注册、命令代次和反馈委托。账号/角色/World切换、EndPlay、提供者撤销与销毁清理对应所有权；迟到或重入回调复核弱引用、认证代次、请求代次及World。新页面只接受同一作用域的初始缓存和后续事件。

## 分配、准入与真实旅行

Match消费者真实启动于HTTP/gRPC组合根。生产步骤存PostgreSQL，按数据库时钟、Version/Fence/Owner租约推进；阶段Outbox同事务，通知不含迁移秘密。Redis原子队列只负责状态投影，不签票。分配先持久固定名单/角色/内容，再真实申请MainArena，逐人签票并保存稳定回执；全员提交前不发布任何准入。

本人迁移接口独立返回短期秘密，适配器只在回调栈移动给RequestTrustedWorldMigration，复用现有RequestWorld/Loading/Session执行链。UI快照不持有端点或TransferTicket。流程独立核对当前认证、Session、WorldDefinition；竞技扩展通过真实Controller/Pawn、角色定义Ready和服务器复制Gameplay资格事件提供就绪事实，不能用按键或本地倒计时提前放行。

单局规则使用后端锁定英雄，全员准入后10秒开赛，20分钟上限。双方各5个唯一出生槽按固定Roster取用。死亡不自动复活，帧末汇合死亡事件后判定剩余队伍，同帧双方全灭或时间到判平。原进程60秒重连复用原Pawn、位置和资源；淘汰者只能恢复连接与观战，不能重新出生。恢复票据使用独立稳定意图，仍单次消费，不把新票等同于重生资格。

## 失败补偿与返回

分配/签票失败撤销所有已持久TicketID，包括响应未知的请求；未分配也保存取消墓碑，迟到成功不能复活该比赛。故障进入Returning后不再次退回补偿阶段。可信结果或故障后，个人返回按原逻辑体验重新寻找安全实例，永不复用旧地址。回收须原Boot真实零人数心跳晚于终局记录，或原实例明确失效；不Drain同名后继Boot。

本地Windows开发使用Deploy/Local/RunMainArenaPool.ps1管理独占Server进程；收到后端released才替换本槽位，仅按PID/启动时间/路径终止自有进程。控制面短暂失联保留实例。生产使用Agones/Fleet；代码与本地进程不构成真实集群验收。

## 当前内容与验证边界

DBAWorldPack_MainArena拥有已保存白盒地图及World/Experience/Pawn定义；五模式为DevelopmentOnly，CustomConfig与-DBADevelopmentArena双开关，Shipping/Test拒绝。DBAArena公共竞技UI通过显式客户端Cook闭包纳入，专服仍剥离UI/表现。生产英雄维护资格提供者仍需正式部署装配，不能把开发白盒声明为ProductionReady。

本轮已执行Go应用/race/vet、真实隔离Redis/PG、HTTP往返及gRPC认证边界回归；十种正常/故障五模式编排包含新会话票据恢复、真实准入和新安全返回。原生、Cook/Stage、容器资源闭包、真实网络及人工视觉的逐阶段结果记录在Docs/Implementation/VillagePartyArena/ExecutionProgress.md。记录之外不宣称通过，也不把双客户端进程启动当作全部多人玩法验收。

## 2026-10-15 审查修正

个人弃权只淘汰本人并在帧末汇合判胜；不能替队友投降。单局断线由独立 SuspendPlayerConnection 关闭输入与施法资格，同一 Pawn / ASC Avatar / 出生代次仍作为受伤目标，死亡订阅继续有效。原 PlayerState 消失后统计进入恢复快照；重连只重新拥有原存活实体，不能重新初始化生命和资源。淘汰、结束与装配销毁撤销目标保留及订阅。

NativeReviewRedActual 的两项实际引擎测试均失败，NativeReviewGreen 的两项均成功；SpawnReentrantSuccessorOwnership 新增使用真实英雄预热和真实角色 Combat 结算的断线死亡分支。此证据不代替物理断网与真实网络客户端重连验收。
