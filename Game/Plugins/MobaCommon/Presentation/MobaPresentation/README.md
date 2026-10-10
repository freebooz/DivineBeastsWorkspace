# MobaPresentation（MOBA表现语义插件）

MobaPresentation位于 MobaCommon（MOBA通用层），职责是把 Arena（竞技）、Combat（战斗）、Ability（技能）、Status（状态）和 Character（角色生命周期）已经确认或已复制的事实转换为平台中立表现请求。

模块：
- MobaPresentationRuntime（MOBA共享表现语义模块）：Runtime（运行时），双端可见、Dedicated Server（专用服务器）安全，只包含GameplayTag（玩法标签）、Context（上下文）、Payload（载荷）、语义注册表和Request Builder（请求构建器）。
- MobaPresentationClient（MOBA客户端表现适配模块）：ClientOnly（仅客户端），按LocalPlayer（本地玩家）隔离事实订阅、去重、Prediction/Confirmation（预测/确认）和平台请求提交。

固定边界：不实现第二套Arena/Combat/Ability/VFX；不直接Spawn Niagara（生成Niagara）、Play Sound（播放音效）、Create Widget（创建界面）；不依赖DivineBeasts项目层；新增Go业务后端接口：无。

当前实现向 UGamePlatformPresentationClientSubsystem（平台表现客户端子系统）提交 FGamePlatformPresentationRequest（平台表现请求）。若没有Provider（表现提供者），安全返回ProviderMissing（提供者缺失），Gameplay继续。

2026-10-09新增 `FMobaHitFeedbackPolicy`（MOBA接触反馈策略），支持轻击/重击/技能/格挡/挥空、连击强度封顶和0/3/6帧参数对比，并增加 `Moba.Presentation.HitFeedback.Policy` 自动化测试源码。Moba客户端接入平台GamePlatformAnimationClient局部视觉顿帧，不依赖DivineBeasts。当前CombatEvent通过平台世界确认事实总线接线；真正的连击输入、攻击类别完整映射和完整九层表现仍为后续任务。策略中保留的历史暴击输入不代表当前GAS已经提供该事实。

验证状态必须区分静态源码、UE5.8编译、Client/Server Cook（客户端/服务器烘焙）、Multi-PIE（多编辑器实例）、Travel/Late Join（切图/晚加入）和压力测试；未执行不得写通过。

本轮事实上下文与迟到接线说明见 [源码整改说明](Docs/AuditRemediation-2026-10-09.md)，UE运行尚待验证。

2026-10-09继续实施：MobaPresentationClient按World范围订阅平台Combat确认事实，并可从项目/竞技组合根注入已经预加载的`UGamePlatformHitFeedbackProfile`、VFX/SFX逻辑DefinitionId；分层使用唯一GamePlatformPresentation调度、GamePlatformAnimationClient Overlay/局部顿帧与GamePlatformCameraClient本地CameraShake。不直接创建Niagara/Sound/Widget或改变服务器击退。已有技能ID默认归Skill反馈，缺失则为Light；真正重击/格挡/连击段数必须由权威规则定义，不根据伤害数值推断。目前项目实例资产和联机仍待验收。当前GAS已移除权威暴击属性，前文历史暴击提示不得视为现行技能实现。

2026-10-09 P5追加`FMobaHitFeedbackResolver`（客户端按命中获取配置的中立回调），每次命中单独解析Profile及VFX/SFX DefinitionId；项目Resolver未命中目录或资源尚未加载时，仅使用中立调校参数，不复用上一击的Profile、VFX/SFX定义或软资产。没有安装项目Resolver时才使用组合根预先配置的默认Profile。Resolver只处理已加载弱引用与值参数，MOBA源码没有引用DivineBeasts项目类型。

整合约束：世界确认事实总线与LocalPlayer/Controller/Pawn迟到接线共用关闭和BindingGeneration（绑定代次）边界；关停、旅行及World退出先解绑两类订阅、取消自有延后Timer。项目命中Resolver只读取已加载缓存，同步回调若关闭或更换配置，旧栈不得继续向下一作用域提交视觉层。Profile不持有第二份资产租约，所有原有受理/缺提供者、去重和世界世代规则仍保留；本段说明不代表UE运行或资产验收完成。

事实扩展与撤账边界（2026-10-09追加）：Context Contributor（上下文贡献者）调用栈持SharedPtr值拷贝，允许同步自注销而不销毁正在执行的对象。提交捕获原LocalPlayer、World、BindingGeneration（本地绑定代次）、ArenaBindingGeneration（同World竞技GameState绑定代次）及独立事实操作身份；每个virtual与平台Submit返回后复查。回调关闭/旅行/重绑时旧事实返回InvalidRequest或StaleWorld，不转交后继World；失败只能撤本次精确操作预约，不能擦后继同FactId或已升级确认的记录。FIFO中的预测确认只保留一次身份，不因旧项驱逐新确认。

竞技批处理先复制Team/Objective数组及比赛身份，跨Recover/Adapt外部返回后核原GameState/World/绑定身份，失效停止后续修订写入与Pawn接线。原0世代自动补齐约定保留，显式旧Identity世代不被Context的0值覆盖。

已有Private/Tests追加真实Native回归注册：Moba.Presentation.Client.ContributorSelfUnregister、ContributorScopeReentry、FactRollbackKeepsSuccessor、ArenaArrayReentry（后3项完整前缀同Moba.Presentation.Client）。夹具通过真实PlayerAdded/两World、贡献者virtual自注销/关停/旅行和实际平台Provider回调验证原栈资格；Array用例分别覆盖首次Bind、Team与Objective批次。Provider仅测试受理与重入，不宣称VFX资源播放。EditorContext | EngineFilter，未执行UE/UBT或运行红绿灯；新私有字段及函数要求消费模块统一重编译。上述初次修复没有新增反射夹具；追加的中立返回状态兼容性见下文。

在途预测/确认受理边界（2026-10-09独立复核追加）：`PendingFactSubmissions`（在途事实操作）与已受理的`PredictedFacts`/`ConfirmedFacts`（预测/确认集合）分离。Provider尚未返回时，同FactId调用返回`Pending`（在途），只有首份确认值快照留存于原预测操作；同步栈持强引用并核原World/绑定/操作身份。原预测真正Submitted后确认只升级一次去重，不二次播放；预测拒绝时先撤原精确预约，再从真实提交入口处理确认。预测与确认均被拒绝不会残留成功账本，后续可真实重试。关闭/旅行及同World竞技GameState重绑均清在途资格；失效返回再按原shared操作/token精确兜底撤账，旧栈不向后继World或绑定排放留存确认，也不擦后继同ID账本。该机制是一个同步栈的有限留存，没有Ticker、第二套异步任务队列或网络状态。

公开返回合同与兼容：`EGamePlatformPresentationSubmitResult`追加`Pending=4`，原Submitted=0、ProviderMissing=1、InvalidRequest=2、StaleWorld=3的身份和值保持。Pending不等于Provider已受理或播放成功；消费方必须单独处理，不能把“未报其他错误”当成功。原同步栈结束后，同身份再次调用可读取已受理去重结果，或在失败/撤销后真实重试；原预测调用始终返回自己的真实结果，不以留存确认的后续结果冒充预测成功。平台Coordinator自身仍同步返回终态；此新状态用于适配层同身份同步重入，未新增通用完成事件。UENUM变更与私有结构变化要求插件及UHT统一重编译，Blueprint枚举Switch/默认分支需显式检查新增状态。源码扫描未发现Shared协议/配置或生产穷举Switch，历史Blueprint资产尚未逐项审定；不宣称Cook/运行兼容已经验证。

新增真实Provider回归：`Moba.Presentation.Client.PredictionRejectedConfirmationReentry`（拒绝预测后确认真实受理）、`Moba.Presentation.Client.PredictionAcceptedConfirmationReentry`（接受预测后确认不双播放）、`Moba.Presentation.Client.PredictionConfirmationRetryAfterRefusal`（两次拒绝后真实重试）、`Moba.Presentation.Client.PredictionPendingScopeCancellation`（关闭/旅行取消留存确认，仅后继World可重新提交）、`Moba.Presentation.Client.PredictionArenaRebindKeepsSuccessor`（真实UWorld.SetGameState通知同World重绑，旧Pending取消，后继同ID实际受理并保留去重）。前置仍为真实LocalPlayer/Coordinator两World夹具，Provider仅Tests模拟受理/拒绝及同步重入；EditorContext | EngineFilter。源码已编写但未执行UE/UBT，不以静态扫描代替红灯/绿灯。
