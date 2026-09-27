# 可玩就绪屏障

| 状态/策略 | 允许可玩 | 具体条件 |
| --- | --- | --- |
| Required | 成功后可参与Ready | 失败立即否决；等待/运行绝不能满足 |
| Optional | 失败不单独否决 | 保留失败状态和错误；第一版等待它的有限终态以完成诊断 |
| Degradable | 主任务或真实Fallback成功 | 没有配置回退拒绝启动；回退失败仍否决 |
| Ready | 是，且资源/目标仍有效 | 所有必需成功、可降级满足、无未结束任务 |
| DegradedReady | 是，且资源/目标仍有效 | 至少一个任务实际走成功回退 |
| Failed | 否 | 必需、回退、必需依赖失败；释放本操作资源 |
| Cancelled | 否 | 调用者取消、释放运行操作或Owner失效 |
| TimedOut | 否 | 到达操作总截止，超时先于同轮尚未接纳的完成 |

任务超时是`TaskTimeout`失败，按本任务策略处理；操作超时不可降级。单轮竞争只接受首个终态。取消不推断网络断开，也不代表远端业务回滚。

基础世界任务必须Required，不允许用Optional/世界回退绕过目标身份。它要求当前GameInstance持有同一UWorld、世界为Game或PIE、已BeginPlay、未bIsTearingDown、去掉PIE前缀后的包名等于目标，并收到当前Loading句柄对应的项目可操作声明。仅地图打开并不满足声明。

Snapshot.State记录本次屏障结果，不代表永远有效。Ready后资源释放、Owner销毁、世界更换或退出，`IsReadyToPlay`立即返回false。上层不能仅从快照中的Ready枚举或100%进度启动玩法。基础模式没有会话任务，不应冒充在线准入；FoundationSessionLoading未执行。

对于《神兽联盟》，建议把“可玩”拆成可验证事实而不是单个 Loading 百分比：会话准入、目标世界事实、必需 Definition、角色/Pawn 与 GAS、玩法规则、必要 UI/HUD 分别由所属系统提供事实或任务。Loading 只聚合这些事实并执行 Required／Optional／Degradable 策略，不拥有这些系统的权威状态。客户端表现类非关键 VFX/SFX 可使用 Degradable/Optional；影响操作、战斗输入或 HUD 基础交互的能力不能为了更快进度被错误标成 Optional。