# NamingValidation（命名验证）

UGamePlatformNamingValidator 使用 UE Data Validation。

默认资产前缀来自 UGamePlatformValidationSettings（游戏平台验证设置），包含 Texture、Material、MaterialInstance、StaticMesh、SkeletalMesh、NiagaraSystem、WidgetBlueprint、Blueprint、Sound。

平台层不强制第三方插件套用项目命名规范。DivineBeasts 项目层后续可通过 Editor 配置覆盖规则，不要求 GameFoundation 反向依赖项目代码。

插件、模块和 C++ 类命名由 CI/架构规则补充检查。