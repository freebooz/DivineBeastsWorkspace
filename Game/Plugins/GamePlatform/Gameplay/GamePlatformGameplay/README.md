# GamePlatformGameplay（游戏平台通用玩法插件）

平台级Gameplay Active（玩法激活）与AvatarGeneration（角色代次）最小权威契约。`UGamePlatformGameplayEligibilityComponent（玩法资格组件）`由服务器修改并复制到客户端，`IGamePlatformGameplayEligibilityProvider（玩法资格接口）`供Interaction等基础插件只读消费。

本轮增加客户端/服务器统一Snapshot与变更事件，复制回调会广播最新状态；无Authority Owner不能伪造Active。登录、Session准入、死亡、重生和队伍仍由对应系统负责，不在本插件复制第二套流程。

本次设计审查整改的真实行为、线程/所有权/失败合同及验证边界见 [2026-09-30专属说明](Docs/DesignRemediation-2026-09-30.md)。其中原生规则测试与UE实际运行分别记录，不混写交付状态。

## 2026-10-09 UE登录初始位置与权威出生边界

UE InitNewPlayer必须先查询真实PlayerStart以初始化Controller位置，之后才发生Gameplay准入握手。FindPlayerStart的非内部调用允许只读选择地图位置，并忽略外部Portal名称；这一步不占用玩法候选、不生成Pawn、不修改Active资格。RestartPlayer、SpawnDefaultPawn、GetDefaultPawnClass及内部候选路径仍使用唯一资格门禁，实际出生仍消费已验证的ReservedSource。

真实瞬态世界回归先复现位置查询返回空，修复后确认可找到真实PlayerStart，外部Restart/Spawn仍无Pawn且无Gameplay Active。Gameplay组5项UE Automation通过；不能由此宣称真实联机或全部角色模式已经验收。
