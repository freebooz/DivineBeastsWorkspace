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

## 8. 深度复审后的当前源码运行模型

2026-09-30已按下面流程接线；真实引擎音频启动/并发拒绝验证尚未执行：

```text
Validate Request
→ Budget Gate（Pending + Active统一预算）
→ Acquire Definition Lease
→ Validate Definition / Assets / Owner
→ Create AudioComponent（不自动播放）
→ 配置Sound/Attenuation/Concurrency/参数/附着
→ 登记Active实例和RequestId
→ 绑定OnAudioFinishedNative / OnAudioPlayStateChangedNative（含启动失败Stopped）
→ Play / FadeIn
→ Stop / FadeOut / Natural Finish
→ 解绑事件 + DestroyComponent + ReleaseDefinition
```

关键原因：生命周期监听必须在播放开始之前建立，不能依赖极短声音“一定来得及”在Spawn后绑定事件。

## 9. 作用域约束

- SFX实例作用域：World；
- Presentation Bridge作用域：LocalPlayer；
- Definition Lease：World；
- 用户音量/静音：Settings/UE混音系统，不复制到SFX配置真源；
- 跨地图持续音乐：不属于本插件；
- Dedicated Server：不创建Subsystem、不链接ClientOnly模块、不Cook纯客户端音频内容；
- 本地分屏：当前尚未形成正式Audience契约，进入正式支持前必须补充WorldShared/LocalPlayer语义和测试。

## 10. 扩展原则

项目层扩展只允许两类内容：

1. 创建 `UGamePlatformSFXDefinition` 数据实例；
2. 在 Presentation Catalog 中注册 SemanticTag → SFX DefinitionId 映射。

不要新增 `UDivineBeastsSFXSubsystem`、生肖专属底层播放器或项目资源硬编码路径。若项目确实需要新的执行维度，应先判断它是否具有跨游戏语义；只有稳定通用能力才向平台层扩展。
