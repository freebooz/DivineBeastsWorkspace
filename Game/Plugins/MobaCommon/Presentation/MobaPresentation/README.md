# MobaPresentation（MOBA表现语义插件）

MobaPresentation位于 MobaCommon（MOBA通用层），职责是把 Arena（竞技）、Combat（战斗）、Ability（技能）、Status（状态）和 Character（角色生命周期）已经确认或已复制的事实转换为平台中立表现请求。

模块：
- MobaPresentationRuntime（MOBA共享表现语义模块）：Runtime（运行时），双端可见、Dedicated Server（专用服务器）安全，只包含GameplayTag（玩法标签）、Context（上下文）、Payload（载荷）、语义注册表和Request Builder（请求构建器）。
- MobaPresentationClient（MOBA客户端表现适配模块）：ClientOnly（仅客户端），按LocalPlayer（本地玩家）隔离事实订阅、去重、Prediction/Confirmation（预测/确认）和平台请求提交。

固定边界：不实现第二套Arena/Combat/Ability/VFX；不直接Spawn Niagara（生成Niagara）、Play Sound（播放音效）、Create Widget（创建界面）；不依赖DivineBeasts项目层；新增Go业务后端接口：无。

当前实现向 UGamePlatformPresentationClientSubsystem（平台表现客户端子系统）提交 FGamePlatformPresentationRequest（平台表现请求）。若没有Provider（表现提供者），安全返回ProviderMissing（提供者缺失），Gameplay继续。

2026-10-09新增 `FMobaHitFeedbackPolicy`（MOBA接触反馈策略），支持轻击/重击/技能/格挡/挥空、暴击、连击强度封顶和0/3/6帧参数对比，并增加 `Moba.Presentation.HitFeedback.Policy` 自动化测试源码。Moba客户端接入平台GamePlatformAnimationClient局部视觉顿帧，不依赖DivineBeasts。当前历史CombatEvent不携带攻击类别时默认轻击；网络权威事实接线、真正的连击输入、完整九层表现仍为后续任务。

验证状态必须区分静态源码、UE5.8编译、Client/Server Cook（客户端/服务器烘焙）、Multi-PIE（多编辑器实例）、Travel/Late Join（切图/晚加入）和压力测试；未执行不得写通过。
