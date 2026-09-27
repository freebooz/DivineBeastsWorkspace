# PresentationIntegration（表现系统集成）

`GamePlatformUIClient`依赖 `GamePlatformPresentationCore（平台表现核心）`，不依赖 VFX/SFX/Animation（特效/音效/动画）具体客户端插件。

本轮为 `GamePlatformPresentation`补充了最小 Runtime 中立契约模块 `GamePlatformPresentationCore`，当前只定义通用 `FGamePlatformPresentationEvent（表现事件）`，用于后续 Toast/Notification 等表现桥接。

依赖方向固定为 UI Client → Presentation Core；Presentation Core 不反向依赖 UI。具体 Provider（提供者）注册应由客户端 Composition Root（组合根）完成。

项目业务页面状态机不能被 Presentation（表现事件）替代。
