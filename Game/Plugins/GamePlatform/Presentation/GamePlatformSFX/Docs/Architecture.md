# GamePlatformSFX 架构设计

版本：0.2.0｜2026-09-29

## 1. 三层位置

`GamePlatformSFX` 属于 GamePlatform（平台层）的 Presentation（表现）分类。依赖方向：

```text
DivineBeasts（项目层）
        ↓
MobaCommon（MOBA通用层）
        ↓
GamePlatformPresentation / GamePlatformSFX / GamePlatformData（平台层）
```

SFX不得反向依赖MobaCommon或DivineBeasts。

## 2. 模块与目标

正式模块只有：

```text
GamePlatformSFXClient — ClientOnly（仅客户端）
TargetAllowList = Client, Editor
```

当前不建立无职责的 Runtime、Server、Editor 空模块。Definition反射类型允许Editor读取，但播放Subsystem只在Game/PIE/GamePreview且非Dedicated Server、非Commandlet环境创建。

## 3. 单一真源

最新架构避免三套重复目录：

- Presentation Catalog：负责语义、Context、项目/内容包覆盖和Provider选择；
- GamePlatformData：负责Definition逻辑身份、版本、依赖和租约加载；
- GamePlatformSFX：只负责音效执行。

因此不存在SFX专属第二套Semantic Catalog。

## 4. 正式调用链

```text
中立Gameplay/Application事实
→ GamePlatformPresentation Request
→ Presentation Catalog解析
→ ProviderChannel=SFX + DefinitionId
→ UGamePlatformSFXPresentationBridgeSubsystem
→ IGamePlatformSFXService
→ UGamePlatformSFXWorldSubsystem
→ FGamePlatformSFXPolicy校验逻辑ID
→ IGamePlatformDataService::AcquireDefinition
→ UGamePlatformSFXDefinition + SFXRuntime Bundle
→ UGameplayStatics / UAudioComponent
→ FGamePlatformSFXHandle
→ OnAudioFinishedNative
→ ReleaseDefinition
```

## 5. 世界与实例边界

World是SFX实例、请求去重和Definition World Lease的生命周期边界。

`FGamePlatformSFXHandle` 包含：Id、Generation、Weak World。旧World的Handle不能控制新World声音。

2D UI提示当前也按World生命周期处理；跨地图音乐属于未来独立音乐导演职责，不强行塞进SFX WorldSubsystem。

## 6. 资源边界

Definition只保存Soft Reference（软引用）。真实声音、衰减、并发资产由 `SFXRuntime` Bundle异步加载。

Dedicated Server不得依赖或Cook纯客户端音频模块和音频内容。Server侧Gameplay结果不能依赖SFX播放是否成功。

## 7. Settings边界

`GamePlatformSettings` 是用户设备/偏好设置真源；`GamePlatformSFX` 是播放执行器。音量、静音和设备偏好应由组合层应用到UE SoundClass/SoundMix/AudioModulation，不让SFX反向依赖Settings，从而避免横向循环。
