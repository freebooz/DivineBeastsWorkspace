# VFXSFXUIBoundary（VFX/SFX/UI边界）

GamePlatformVFX（平台VFX）仍是唯一Niagara运行框架。MobaPresentation源码禁止直接调用 UNiagaraFunctionLibrary::SpawnSystem*。

MobaPresentation不Load/Play Sound（加载/播放音效）、不操作Audio Component（音频组件）、不CreateWidget（创建界面）、不直接CameraShake（镜头震动）。

Damage Number、Kill Feed、Score Popup等由上层UI/Presentation Provider消费中立语义或Arena事实。

当前GamePlatformVFX已有 FGamePlatformVFXPresentationProvider（VFX表现提供者）适配类，但尚未正式注册到平台Coordinator；Provider缺失时本插件安全降级，不影响Gameplay。
