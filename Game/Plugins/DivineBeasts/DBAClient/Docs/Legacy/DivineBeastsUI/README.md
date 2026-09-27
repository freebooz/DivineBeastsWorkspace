# DivineBeastsUI（神兽联盟项目UI插件）

路径：Game/Plugins/DivineBeasts/Presentation/DivineBeastsUI。

本插件只有一个 DivineBeastsUIClient（神兽联盟项目UI客户端模块），类型为 ClientOnly（仅客户端）。正式最小依赖只有 DivineBeastsRuntime（神兽联盟项目核心）与 GamePlatformUIClient（游戏平台UI客户端模块）；不直接依赖 DivineBeastsApplicationFlowClient（项目应用流程实现模块）、DivineBeastsArenaClient（项目竞技客户端实现模块）、后端 Contracts（契约）或后端服务。

UI数据流固定为：Widget（界面控件）→ ViewModel（视图模型）→ IDivineBeastsUICommandPort（UI命令端口）→ 业务Owner（所有者）→ Authority Result（权威结果）→ IDivineBeastsUIQuerySource（UI查询源）→ View State（视图状态）→ Widget。UI不是Gameplay（玩法）或Backend（后端）真源。

项目复用 UGamePlatformUIManagerSubsystem（平台UI管理子系统）、UGamePlatformUIScreenDefinition（平台页面定义）、CommonUI（通用UI）页面栈、平台 Loading Screen Service（加载界面服务）和 UGamePlatformViewModelBase（平台视图模型基类）。没有创建第二套UI Manager、Page Stack（页面栈）或Input Router（输入路由器）。

DBAClient（神兽联盟客户端组合层）的 UDBAUICompositionSubsystem（UI组合根适配器）同时观察ApplicationFlow/Arena/Loading/Combat/Interaction（应用流程/竞技/加载/战斗/交互）并实现UI Query/Command契约，因此UI模块本身不反向依赖业务实现。

当前实际UI二进制资产数量为0；WBP_UI_*（界面蓝图）、持久化 UGamePlatformUIScreenDefinition（页面定义资产）、CommonUI Root Layout（根布局）和 DBAUIPack_Core（公共UI资产包）必须由UE5.8 Unreal Editor（虚幻编辑器）合法创建。本轮没有伪造 .uasset（虚幻资产）。

新增Go业务后端接口：无。新增Go微服务：无。
