# GamePlatformGameplay（游戏平台通用玩法插件）

平台级Gameplay Active（玩法激活）与AvatarGeneration（角色代次）最小权威契约。`UGamePlatformGameplayEligibilityComponent（玩法资格组件）`由服务器修改并复制到客户端，`IGamePlatformGameplayEligibilityProvider（玩法资格接口）`供Interaction等基础插件只读消费。

本轮增加客户端/服务器统一Snapshot与变更事件，复制回调会广播最新状态；无Authority Owner不能伪造Active。登录、Session准入、死亡、重生和队伍仍由对应系统负责，不在本插件复制第二套流程。

本次设计审查整改的真实行为、线程/所有权/失败合同及验证边界见 [2026-09-30专属说明](Docs/DesignRemediation-2026-09-30.md)。其中原生规则测试与UE实际运行分别记录，不混写交付状态。
