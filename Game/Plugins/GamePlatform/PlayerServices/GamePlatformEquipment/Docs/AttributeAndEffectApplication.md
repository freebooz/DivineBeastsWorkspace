# AttributeAndEffectApplication（属性与效果应用）

装备属性必须通过 GameplayAbilitySystem（玩法能力系统）的 GameplayEffect/AbilitySet 表达。Equipment 不调用 SetNumericAttributeBase，也不写 Attack += 50 等第二套属性系统。

最终 Damage、Defense、Crit、Resistance 等计算仍由 AbilitySystem/Combat（能力系统/战斗）正式公式解释。

纯视觉装备可以没有 AbilitySet/Effect；有 Gameplay 影响的装备必须等待真实 Resolver 和资源验证。