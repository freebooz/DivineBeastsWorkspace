# AnimationBoundary（动画边界）

本插件不创建Animation Runtime Framework（动画运行框架），不绑定AnimBlueprint、Montage或动画资源。

角色组件只提供Hero/Zodiac/Generation/Readiness等公开事实。后续DivineBeastsPresentation或Animation扩展根据这些事实选择表现资产。

死亡/复活动画只消费Combat/Gameplay生命周期事实；Characters不自行结算死亡，也不在死亡后启动Timer自我Spawn。
