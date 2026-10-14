# SSLM组队与竞技页面原生命令桥

## 职责与依赖

公共小队DTO及类型化请求归GamePlatformOnline；项目公共ViewModel、页面路由、列表条目和命令接线归DBAClient/DivineBeastsUIClient。公共代码不包含MobaCommon或DBAArena头文件。五页公共资产归DBAUIPack_Core，竞技四页归DBAArena；没有新增插件、模块或第二套网络执行器。

竞技客户端通过`IGamePlatformArenaMatchmakingAdapter`消费MOBA公开端口。`UDivineBeastsArenaUIClientSubsystem`是每LocalPlayer的项目组合点，拥有提供者注册句柄、在途命令代次及只读反馈事件，不拥有后端队列、名单锁、确认权威或准入签名。真实生产适配器必须由竞技组合根提供，不能只因有创建接口就宣称完整生命周期可用。

## 公开命令与失败边界

- `RegisterMatchmakingAdapter`仅游戏线程，首次注册返回唯一FGuid，重复提供者不覆盖；`UnregisterMatchmakingAdapter`只接受匹配的非空句柄。撤销令旧回调失效，不提交远端取消。
- `RequestMatchmaking`接收真实目录ModeId，当前页面打开时携带授权小队PartyId；无队伍才允许空PartyId。模式正式配置、认证代次和完整后端生命周期任一缺失均返回false且不产生远端副作用。返回true只表示交给提供者，最终成功等待事件。
- `CancelMatchmaking`沿用报名逻辑身份；只有远端确认才清本地报名身份和显示取消。关页、销毁World或取消本地等待均不等于释放远端队伍锁。
- `RespondToMatchOffer`接受布尔意图，提交适配器授权CandidateId、非负Revision及独立ClientRequestId。不存在身份、版本、权威截止时间或回复资格时拒绝。该路径不调用局内SetArenaReady，不直接ClientTravel。
- 回调只允许游戏线程，并检查弱对象、命令代次、当前认证主体与AuthGeneration；世界退出、控制器变化、账号退出/重登和提供者撤销均使旧代次失效。重复回调不会再次发布终态。
- 远端任意错误正文不进入Widget或遥测。请求未获确认时不显示“成功”；报名逻辑身份保留，阻止生成新身份盲目重放。

## 界面合同

公共页面只读UDivineBeastsPartyViewModel，按钮提交类型化普通小队命令，列表使用UDivineBeastsPartyListEntry。名单、邀请和申请的稳定选择键来自当前授权快照，不默认选择首项。解散需要复选确认；旧名单在读取失败后不能授予写权限。

竞技模式列表展示实际目录的1v1、2v2、3v3、4v4、5v5 TeamSize，不从协议枚举推算。候选确认与场内准备分离；竞技HUD显示复制记分板，生命/气势、Buff、施法和技能栏仍由公共RootLayout已有CombatHUD负责，避免第二套状态或播放器。死亡次数不冒充权威淘汰事实。

## 当前后端与发布限制

当前实际Gateway只有创建匹配票据路由；没有完整观察、取消、逐人候选确认合同和生产客户端适配器。`SupportsTicketLifecycle`默认false，本次没有伪造路由、返回值或生产注册，因此正式开始/取消/接受/拒绝维持不可用。五模式实际目录仍为NotConfigured，未把开发地图改成ProductionReady。

普通小队限时准备会话、未建队原子邀请、世界右键目标到上下文菜单的调用方，以及竞技权威淘汰事实仍需后续真实集成。原生命令入口和蓝图完成不意味着这些能力完成，不作为可上线闭环交付。

## 验证边界

独立审查后的同批修复包括：公共页面按平台具体实例/原栈账本退休绑定、同Widget换模型防护；成员/邀请/模式改接SelectionChanged并在重建期间防重入，提交前再核真实选中对象；只有服务端确定回复且状态Idle的匹配逻辑身份才释放。新增SelectionAndRecycledBinding、KeyboardModeSelection和ConfirmedOfferTerminal原生回归，不把测试替身/内存Widget夹具冒充生产资产或多人联机。

Editor集中构建、原生自动化及Monolith编译/保存/冷读证据按本轮执行台账登记。新增测试替身仅在Private/Tests且WITH_DEV_AUTOMATION_TESTS中存在，不接入生产。没有运行Go、Client/Server构建、Cook/Stage、多人网络或人工视觉验收；禁止从静态可访问性零警告推断设备适配和颜色已验收。
