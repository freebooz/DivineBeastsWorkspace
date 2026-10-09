# GamePlatformAbilitySystem（游戏平台技能系统插件）

2026-10-09 **最新无韧性实施规范**：`Docs/GAS无韧性精简属性实施规范_20261009.md`（4个属性集、17字段、4公开/11拥有者/2不复制），移除控制韧性/失衡属性与控制时长减免公式。`Docs/GAS精简属性集与复制分层实施规范_20261009.md`（此前18字段方案）和`Docs/GAS属性集设计与实施建议_20261009.md`（24字段历史方案）均为历史快照，不得再次恢复控制属性集；原生测试和联机实测按最新规范验收。

跨游戏 GAS（Gameplay Ability System，玩法能力系统）基础层：提供 `UGamePlatformAbilitySystemComponent（平台ASC基类）`、`UGamePlatformGameplayAbility（平台玩法能力基类）`、`UGamePlatformAttributeSet（平台属性集基类）`、AbilitySet授权合同、中立InputTag及 `IGamePlatformAbilityInputReceiver（平台能力输入接收器）`。

当前ASC统一管理Owner/Avatar ActorInfo、AvatarGeneration代次和输入作用域。重生/换Pawn会先清理旧按压状态并使旧Token失效；能力输入只允许本地拥有者，以 `Platform.Ability.Input.*` 精确匹配唯一Spec，重复InputTag Fail Closed。`UGamePlatformGameplayAbility` 显式提供 OnPressed / WhileHeld 激活策略，输入层不根据项目技能名称猜测策略。

平台层仍不定义具体生命、法力、生肖技能、伤害公式、按键或MOBA规则；具体 Ability Grant、Cooldown、Cost和项目技能由项目Definition/派生类型扩展，不反向依赖Combat或DivineBeasts。

`UGamePlatformAbilitySetDefinition::ValidateDefinition（能力集定义校验）` 现已实现纯字段门禁：校验条目容量、逻辑ID/软类唯一性、等级范围、`Platform.Ability.Input.*` 输入标签唯一性和属性集类唯一性；该阶段不加载软资源、不访问世界、不产生GAS副作用，真实类型/网络策略/效果可回滚性继续由ASC授权阶段验证。

2026-10-09 专项增量：`UGamePlatformAbilitySystemComponent`（平台技能系统组件）现在覆盖 UE 原生 `OnRep_ActivateAbilities`（技能授权列表复制完成），调用父类后发送无项目语义的 `OnAbilitySpecListChanged`（原生技能规格列表变更）C++ 委托；技能 UI 领域适配据此处理 OwnerOnly（仅拥有者）自定义技能槽状态先于原生 AbilitySpec 复制到达的乱序情况。公开接口不包含十二生肖类型、具体 UI、Cooldown 业务或客户端资产；订阅调用方必须解绑。此代码已写入源码，尚待锁定 UE5.8 的模块编译和复制回归，不能宣称正式运行通过。
