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
