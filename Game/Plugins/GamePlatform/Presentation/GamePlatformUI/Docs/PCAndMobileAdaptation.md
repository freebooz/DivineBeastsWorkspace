# PCAndMobileAdaptation（PC与移动端适配）

平台层只提供跨游戏机制：Keyboard/Mouse（键鼠）、Gamepad（手柄）、Touch（触控）输入模式，DPI Scale（DPI缩放）、Safe Zone（安全区）、Aspect Ratio（宽高比）和输入提示/图标的承载接口；具体项目布局不属于GameFoundation（游戏平台基础层）。

Screen Definition（页面定义）支持 PlatformWidgetVariants（平台控件变体）。Manager使用 FPlatformProperties::IniPlatformName（平台配置名）选择软类变体，而不是通过游戏业务判断平台。

平台变体只能改变UI表现、布局和资源，不能改变Gameplay（玩法）规则或服务器决策。PlatformWidgetVariants仍经过与默认WidgetClass相同的安全软路径/页面类校验。

Android（安卓）触控目标尺寸、横竖屏、异形屏Safe Zone、Mobile HUD Density（移动HUD密度）及真实输入图标切换必须在设备/模拟器中验证；当前状态为未执行。

