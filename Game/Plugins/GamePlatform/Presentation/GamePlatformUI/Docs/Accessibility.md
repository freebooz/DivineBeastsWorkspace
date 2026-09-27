# Accessibility（可访问性）

平台源码提供 FGamePlatformUIAccessibilityPreferences（UI可访问性偏好）与 UGamePlatformUIManagerSubsystem（UI管理子系统）统一入口，包含 TextScale（文本缩放）、TouchTargetScale（触控目标缩放）、bReducedMotion（减少动态效果）、bPreferHighContrast（高对比度偏好）和 bRequireNonColorStatusCues（非颜色状态线索要求）。Blueprint/Style（蓝图/样式）可订阅 OnAccessibilityPreferencesChanged（可访问性偏好变化事件）应用实际视觉样式。

SetAccessibilityPreferences（设置可访问性偏好）会保证文本缩放不低于0.5、触控目标不小于默认倍率1.0；ResolveTransition（解析过渡）在 Reduced Motion 开启时把 Default（默认动画）降级为 Instant（即时切换），而显式 None/Instant 保持不变。

具体 Contrast（对比度）、字号视觉结果、触控物理尺寸和 Style（样式）仍需要 Editor UI 资产后验证；当前源码只提供策略钩子，不能据此声称真实视觉对比度或设备触控尺寸已经达标。

Gamepad/Keyboard 焦点路径通过 CommonUI DesiredFocus（期望焦点）机制接入，实际页面必须为每个主要状态提供可达焦点。

本轮不承诺尚未实现或未验证的 Screen Reader（屏幕阅读器）支持。
