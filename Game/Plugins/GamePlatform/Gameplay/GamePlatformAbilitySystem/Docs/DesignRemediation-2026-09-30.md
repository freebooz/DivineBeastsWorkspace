# F22 能力激活资格门禁（2026-09-30）

平台运行模块只拥有 GAS ActorInfo、输入和本ASC注入的只读Gate，不认识项目生肖、MOBA或Gameplay服务。项目组合根在 ActorInfo 就绪后用 SetActivationGate(WeakOwner,SharedGate) 注入一个读取器；Owner需同世界且存活，别的Owner不能覆盖。ClearActivationGate只能撤销自身注册。换Avatar先清理输入和旧Gate，再建立新ActorInfo、增加本地代次并广播 OnAvatarBindingChanged，组合根在该事件后重新注入。

EvaluateActivationEligibility 先核对绑定、弱Owner、Gate和Avatar代次，再同步调用现有 IGamePlatformAbilityActivationGate::Evaluate(const ASC&) 返回 FGamePlatformResult；未注入、过期、重入或查询期间换Avatar均失败关闭。Gate不拥有复制状态、不请求网络、不假定固定成功。所有注册、查询和撤销限定游戏线程。

输入 Pressed/Process 的真实激活前会查询Gate；Release仍允许清理按压状态。UGamePlatformGameplayAbility 最终覆盖 CanActivateAbility，所有该平台基类的 GAS 尝试先查Gate再执行原生标签、费用、冷却和蓝图资格。直接继承原生UGameplayAbility的外部代码不由此基类保护，需采用平台基类或自行加入相同门禁。已有源码没有C++子类覆盖该函数；final是兼容影响，未来额外C++资格应通过GAS标签/K2入口表达。

涉及公开ASC/GameplayAbility与Private实现、Activation/AbilityActivationPolicy.h、Private/Tests/AbilityActivationGateTests.cpp/AbilityActivationPolicyTests.cpp。纯策略测试覆盖缺Gate、旧代次；UE测试真实绑定ASC并换Avatar，注入的布尔测试Gate仅存在测试源，不能冒充项目权威。项目真实组合根在DBAGameplay角色组件，详见其专属说明。

## 验证与中文审核边界

本次修改已补上述责任、端侧、游戏线程、所有权、失败和取消合同；新增C++测试放在模块Private/Tests，插件Tests只持原生编译入口。原生结果与UE Automation/Editor/Client/Server编译分别记录于Game/Saved/Reviews/task3-repair-report.md，不能将纯规则通过当引擎时序通过。存量公开类型仍有逐字段中文说明缺口，本页不宣称全插件或全工作空间已经整体合规。
