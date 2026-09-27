# SecurityAndAuthority（安全与权威）

UI是不可信客户端。RequiredTags/BlockedTags（所需/阻止标签）、按钮Disabled（禁用）和页面可见性只能改善体验，不能替代Server（服务器）权限检查。

当前运行时代码已落实本地资源边界：UIScreenDefinition（页面定义）注册/打开会校验 WidgetClass、PreloadAssets 和 PlatformWidgetVariants（平台控件变体）。未加载软引用必须是合法本地Cook Package（烘焙包）路径；拒绝外部URL、file:、路径穿越、非法Package和未加载 /Script/ 类路径。异步加载完成后还会再次确认实际UClass不是Abstract（抽象类）且继承 UGamePlatformUIScreen（平台页面）。

平台API不接受业务服务返回的任意 WidgetClass Path（控件类路径）、Asset Path（资产路径）或External URL（外部URL）直接实例化。正式页面只能来自本地注册Definition。

Widget/ViewModel（控件/视图模型）不能执行SetHealth、GiveAbility、SetScore、CompleteQuest等权威修改；只能通过上层Query/Intent（查询/意图）模式与业务Owner交互。

敏感Password/Token/Ticket（密码/令牌/票据）不得存入通用ViewModel、日志、Toast或持久UI数据。Debug页面在Shipping注册阶段直接拒绝。

