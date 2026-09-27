# PresentationIntegration（表现层集成）

目标调用方向：Gameplay / GAS（玩法/技能）→ GamePlatformPresentation（平台表现协调）→ FGamePlatformVFXPresentationProvider（VFX表现提供者）→ IGamePlatformVFXService（VFX服务）。

当前 Provider 已将 SemanticTag、Context、SpawnContext 和 Parameters 转换为 VFX Request。

当前限制：GamePlatformPresentation 仍为工程骨架，尚无正式 Provider 注册协议，因此本插件不反向修改或伪造其接口。待其公共表现 Provider 契约实现后，只需在模块启动阶段完成注册，不改变 VFX 内部运行链。
