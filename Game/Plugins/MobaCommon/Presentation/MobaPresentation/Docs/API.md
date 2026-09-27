# API（接口）

主要公共类型：
- FMobaPresentationContext（MOBA表现上下文）：稳定、结构化表现上下文。
- FMobaPresentationAdaptedFact（已适配表现事实）：Semantic（语义）+ Context（上下文）+ Fact Identity（事实身份）。
- IMobaPresentationContextContributor（上下文贡献接口）：允许上层单向补充项目上下文。
- FMobaPresentationSemanticRegistry（语义注册表）：声明语义所有权、事实来源和瞬时/持续属性。
- FMobaPresentationRequestBuilder（平台请求构建器）：构建 FGamePlatformPresentationRequest（平台表现请求）。
- UMobaPresentationClientSubsystem（MOBA客户端表现适配子系统）：LocalPlayer作用域的事实适配、去重和提交入口。

客户端还公开 AdaptCriticalFact、AdaptAbilityFact、AdaptStatusFact、AdaptCharacterFact、AdaptArenaFact 与 RecoverPersistentFact（恢复持续事实）。
