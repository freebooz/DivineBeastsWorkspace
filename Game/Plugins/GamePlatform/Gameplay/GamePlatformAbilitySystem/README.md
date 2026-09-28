# GamePlatformAbilitySystem（游戏平台技能系统插件）

跨游戏 GAS（Gameplay Ability System，玩法能力系统）基础层：提供 `UGamePlatformAbilitySystemComponent（平台ASC基类）`、`UGamePlatformGameplayAbility（平台玩法能力基类）`、`UGamePlatformAttributeSet（平台属性集基类）`、AbilitySet授权合同、中立InputTag及 `IGamePlatformAbilityInputReceiver（平台能力输入接收器）`。

当前ASC统一管理Owner/Avatar ActorInfo、AvatarGeneration代次和输入作用域。重生/换Pawn会先清理旧按压状态并使旧Token失效；能力输入只允许本地拥有者，以 `Platform.Ability.Input.*` 精确匹配唯一Spec，重复InputTag Fail Closed。`UGamePlatformGameplayAbility` 显式提供 OnPressed / WhileHeld 激活策略，输入层不根据项目技能名称猜测策略。

平台层仍不定义具体生命、法力、生肖技能、伤害公式、按键或MOBA规则；具体 Ability Grant、Cooldown、Cost和项目技能由项目Definition/派生类型扩展，不反向依赖Combat或DivineBeasts。

`UGamePlatformAbilitySetDefinition::ValidateDefinition（能力集定义校验）` 现已实现纯字段门禁：校验条目容量、逻辑ID/软类唯一性、等级范围、`Platform.Ability.Input.*` 输入标签唯一性和属性集类唯一性；该阶段不加载软资源、不访问世界、不产生GAS副作用，真实类型/网络策略/效果可回滚性继续由ASC授权阶段验证。
