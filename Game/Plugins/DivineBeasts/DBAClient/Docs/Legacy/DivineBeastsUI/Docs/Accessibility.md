# Accessibility（无障碍）

一期必须检查的方向：

- contrast（对比度）
- keyboard/gamepad focus（键盘/手柄焦点）
- font scale（字体缩放）
- touch target（触控目标尺寸）
- not color-only（不能只靠颜色表达）
- reduced motion（降低动效）

源码已经为关键Screen提供DefaultFocus目标，并通过平台CommonUI/Input体系预留多设备导航。

但当前没有真实Widget、Style、Font或Android资源，因此对比度、字体缩放、触控面积、色觉友好和降低动效的视觉验收全部为“未执行”。

后续人工审查必须基于UE Editor中的真实页面，而不是只看C++清单。
