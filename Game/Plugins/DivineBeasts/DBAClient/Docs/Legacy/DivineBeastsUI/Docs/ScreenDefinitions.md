# ScreenDefinitions（页面定义）

所有正式Screen（页面）都通过平台 UGamePlatformUIScreenDefinition（页面定义）注册到 UGamePlatformUIManagerSubsystem（UI管理子系统），业务代码不直接 CreateWidget + AddToViewport（创建控件并添加视口）。

FDivineBeastsUIScreenCatalog（项目页面清单）当前声明：

- ScreenId（页面ID）
- WidgetClass软路径
- Layer（层）
- InputMode（输入模式）
- PausePolicy（暂停策略）
- Transition（过渡）
- DefaultFocusWidgetName（默认焦点）
- PC默认Widget路径
- Android Widget variant（安卓界面变体）
- Travel存活策略

当前 UDivineBeastsUIClientSubsystem 根据清单创建Transient（瞬时）平台Screen Definition并注册；这是源码层页面定义实现。真正持久化 DA_UI_*（页面定义数据资产）和 WBP_UI_*（页面蓝图）当前数量为0，必须在UE5.8 Editor中生成，状态为“未执行”。

不存在真实Widget资产时，OpenScreenAsync（异步打开页面）将无法完成实际界面加载；静态注册通过不能冒充页面运行通过。
