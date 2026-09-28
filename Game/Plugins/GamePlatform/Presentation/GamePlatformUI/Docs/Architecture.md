# Architecture（总体架构）

`GamePlatformUI（游戏平台UI插件）`是 `GameFoundation（游戏平台基础层）`的跨游戏 UI 框架，只允许一个 `GamePlatformUIClient（游戏平台UI客户端模块）`，类型为 `ClientOnly（仅客户端）`。

运行结构：`LocalPlayer（本地玩家） → UGamePlatformUIManagerSubsystem（UI管理子系统） → Service（通知/反馈/世界UI/加载服务） → UGamePlatformUILayerStack（层栈） → Widget/Screen（控件/页面）`。Screen/Modal/System/Loading/Debug 使用 `CommonUI Activatable Stack（CommonUI可激活页面栈）`；HUD/WorldProjection/Feedback/Notification 使用普通覆盖层。

页面不直接创建后端请求，也不拥有 Gameplay（玩法）权威状态。平台只提供页面生命周期、异步资源、输入、焦点、导航、加载、对话框、通知、对象池化高频反馈、集中世界投影和通用视觉原子的跨游戏机制。

直接公共依赖固定为 `GamePlatformCore（平台核心）`、`GamePlatformData（平台数据）`、`GamePlatformInputClient（平台输入客户端）`、`GamePlatformPresentationCore（平台表现核心）`；禁止引用 `DivineBeasts（神兽联盟项目层）`和 `MobaCommon（MOBA通用层）`。
