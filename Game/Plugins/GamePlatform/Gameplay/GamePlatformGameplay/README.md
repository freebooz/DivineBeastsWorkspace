# GamePlatformGameplay（游戏平台通用玩法插件）

本插件已有两种可复用契约：完整服务器体验/玩家生命周期框架，以及可被独立玩法组合的最小Gameplay Active/AvatarGeneration资格组件。服务器是资格写入权威，客户端读取复制快照并消费状态变化事件。

`AGamePlatformGameModeBase`拥有当前世界的已验证准入、准备令牌、出生门禁、占位与排空；`AGamePlatformGameStateBase`和`AGamePlatformPlayerStateBase`承载只读复制事实。出生默认入口全部汇入唯一门禁，项目不得绕过final入口直接指定未经批准的位置。账号认证与全球实例分配仍归后端和Server/Session适配，角色身份与定义配置归Character。

`UGamePlatformGameplayEligibilityComponent`是可组合的最小事实载体，不另执行登录/匹配/出生流程。独立竞技宿主通过其服务器生命周期适配设置Active并绑定正整数Avatar代次，死亡、断线与结束立即失活。能力与交互只读消费，客户端不能写入权威状态。

完整体验框架目前通过0.05秒有界Timer推进在途准备/出生状态并检查超时；不得因此宣称全部实现事件驱动或已经完成联机验收。规则与验证边界见[架构说明](Docs/Architecture.md)、[2026-09-30整改说明](Docs/DesignRemediation-2026-09-30.md)。
