# ScreenDefinitions（页面定义）

UGamePlatformUIScreenDefinition（游戏平台页面定义）是 UPrimaryDataAsset（主数据资产），字段包括 ScreenId（页面编号）、WidgetClass（控件软类）、Layer（层）、InputMode（输入模式）、PausePolicy（暂停策略）、Transition（过渡）、PreloadAssets（预加载资产）、Required/Blocked Tags（所需/阻止标签）、DefaultFocusWidgetName（默认焦点）、bSurvivesTravel（跨地图保留）和 PlatformWidgetVariants（平台控件变体）。

注册、打开和异步加载完成三个阶段都会执行/复核结构安全。当前 ValidateDefinition（定义校验）要求：

- ScreenId不能为空且不能重复注册。
- WidgetClass必须是可实例化的已加载 UGamePlatformUIScreen（平台页面）派生类，或合法的本地Cook内容 Soft Class Path（软类路径）；已加载但带 CLASS_Abstract（抽象类标记）的页面类在注册前直接拒绝。
- 拒绝包含 ://、..、file: 的外部/穿越路径，也拒绝未加载的 /Script/ 类路径。
- Screen Definition只能使用 Screen、Modal、System、Loading、Debug 可激活层；HUD/Notification走各自Overlay（覆盖层）入口。
- Shipping（正式发布）禁止Debug页面。
- Screen/Modal/System中只要不是GameOnly，就必须声明 DefaultFocusWidgetName，避免手柄/键盘页面无焦点。
- RequiredTags与BlockedTags不能重叠；它们只控制UI体验，不代替Server权限。
- PreloadAssets和PlatformWidgetVariants必须使用安全的本地软引用；平台变体Key不能为空。
- bSurvivesTravel默认false，仅明确属于LocalPlayer全局UI的页面才允许开启。

WidgetClass和PreloadAssets仍统一由 GamePlatformData（平台数据）的 FGamePlatformAssetLoader（平台资产加载器）异步加载；平台UI没有第二套AssetManager，也没有同步大资源加载路径。

真实 Screen Definition / Widget Blueprint（二进制页面定义/控件蓝图）当前仍为0，因此上述源码与静态门禁通过不等于真实页面加载通过。

