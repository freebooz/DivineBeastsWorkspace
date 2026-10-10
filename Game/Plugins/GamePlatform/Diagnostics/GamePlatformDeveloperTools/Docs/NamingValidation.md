# NamingValidation（命名验证）

UGamePlatformNamingValidator 使用 UE Data Validation。

默认资产前缀来自 UGamePlatformValidationSettings（游戏平台验证设置），包含 Texture、Material、MaterialInstance、StaticMesh、SkeletalMesh、NiagaraSystem、AnimBlueprint（ABP_）、WidgetBlueprint（WBP_）、GameplayAbility（GA_）、GameplayEffect（GE_）、Blueprint（BP_）、Sound。

`ResolveAssetPrefix`在游戏线程只读已加载对象及本配置，不加载项目资源，也不修改资产。动画蓝图与Widget蓝图按真实资产类型优先匹配；普通UBlueprint从`ParentClass`沿真实祖先链查找配置，最近祖先优先，同一祖先的完整反射路径优先于精确短类名，最后回退`Blueprint`。例如继承引擎`GameplayAbility_CharacterJump`的蓝图仍匹配祖先`GameplayAbility`的GA_，效果蓝图匹配GameplayEffect的GE_；不能把所有UBlueprint都要求成BP_，也不能从资产当前名称猜测领域。空对象或无规则返回空前缀，配置空前缀表示不要求前缀。其他资产保留既有固定顺序的类型关键字规则，匹配不依赖TMap遍历顺序。

平台层不强制第三方插件套用项目命名规范。DivineBeasts 项目层可通过 Editor 配置覆盖`AssetClassPrefixes`，无需让GamePlatform反向依赖项目代码；可选GAS领域通过反射祖先名称识别，不给开发者工具增加GameplayAbilities链接依赖。配置只改变命名审核，不改变资源挂载点、父类、已发布资产身份或引用。

回归`GamePlatform.DeveloperTools.Naming.BlueprintParentPrefixes`使用瞬态蓝图对象与引擎真实类，覆盖GA/GE领域、中间原生派生、最近祖先与完整路径优先、配置覆盖、普通蓝图与空父类、ABP/WBP专用资产优先，以及空对象。GAS和UMGEditor类型必须在该测试宿主中可加载，缺失时测试明确失败；不会保存uasset或修改默认配置。源码与静态差异检查不能替代本轮锁定UE编译及真实自动化执行结果。

插件、模块和 C++ 类命名由 CI/架构规则补充检查。
