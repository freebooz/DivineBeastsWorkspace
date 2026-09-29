# GamePlatformSFX 实现规格

版本：0.2.0｜2026-09-29

> 状态：核心机制源码已实施；编译、UE Automation、真实音频资产和5v5性能证据以 `TestingAndEvidence.md` 为准。

## 1. 最新定位

`GamePlatformSFX` 是 GamePlatform（平台层）的客户端通用音效执行插件。三层依赖只能是：

```text
DivineBeasts → MobaCommon → GamePlatformSFX / GamePlatformPresentation / GamePlatformData
```

平台层不认识生肖、英雄、技能名、竞技模式或项目资源目录。

## 2. 本轮关键设计修正

旧规格计划建立 `UGamePlatformSFXCatalog + Resolver`。本轮审查认为该设计会与已有两个稳定真源重叠：

1. `GamePlatformPresentation` 已负责 SemanticTag（表现语义）→ ProviderChannel + DefinitionId；
2. `GamePlatformData` 已负责规范逻辑ID → 唯一 Definition 主资产及异步租约。

因此最新设计删除第二套SFX语义目录，形成：

```text
Presentation Catalog
→ ProviderChannel=SFX + DefinitionId
→ SFX Service
→ GamePlatformData Lease
→ SFX Definition
→ UE Audio
```

这样避免语义冲突、双缓存、双优先级和内容包覆盖顺序不一致。

## 3. Definition合同

`UGamePlatformSFXDefinition : UGamePlatformDefinitionBase`。

当前字段：

- `Sound`：`TSoftObjectPtr<USoundBase>`，可指向 SoundWave、SoundCue、MetaSound Source；
- `Attenuation`：UE原生衰减资产软引用；
- `Concurrency`：UE原生声音并发资产软引用；
- `PlaybackSpace`：TwoD / World / Attached；
- `bStopWhenOwnerDestroyed`：附着目标销毁策略；
- `VolumeMultiplier / PitchMultiplier`；
- `FadeInSeconds / FadeOutSeconds`；
- `AllowedFloatParameters`：MetaSound/SoundCue请求参数白名单；
- `DefaultFloatParameters`：Definition默认参数。

上述音频软资源统一声明 `SFXRuntime` Asset Bundle，由 `GamePlatformData` 租约加载和持有。

## 4. 运行时合同

`IGamePlatformSFXService` 提供：

- `Play`：提交播放；
- `Stop`：按实例句柄停止；
- `StopByRequestId`：按Presentation请求身份取消；
- `IsActive`；
- `SetFloatParameter`；
- `SetVolumeMultiplier`；
- `GetDiagnostics`。

`FGamePlatformSFXHandle` 使用 Id + Generation + Weak World，阻止旧世界/旧句柄误操作。

## 5. 生命周期

当前SFX范围只处理**当前World内的瞬时或循环音效**，包括2D UI提示、世界音效和附着音效。跨地图持续的音乐导演不放进WorldSubsystem，本轮明确不实现。

音频生命周期：

```text
Play
→ 申请GamePlatformData World Lease
→ 异步完成
→ 取得Definition及SFXRuntime Bundle
→ 创建AudioComponent
→ OnAudioFinishedNative
→ 移除实例
→ ReleaseDefinition
```

World销毁时停止活动组件、释放待加载和活动租约，不依赖Tick扫描。

## 6. UE原生能力优先

平台不重写：

- Audio Mixer；
- SoundConcurrency；
- Attenuation；
- SoundClass / SoundMix；
- MetaSound执行器；
- Audio Device管理。

声音分组与全局音量仍应由UE原生SoundClass/SoundMix/AudioModulation以及 `GamePlatformSettings` 的用户偏好适配完成。SFX插件不成为设置真源。

## 7. Presentation边界

`UGamePlatformSFXPresentationBridgeSubsystem` 仅注册 `ProviderChannel=SFX`。

- Predicted：转换为SFX Request；
- Confirmed：沿用同 `RequestId` 去重，避免预测与确认重复发声；
- Corrected：先按 `RequestId` 立即停止旧预测实例，再按纠正后的Definition/位置重新播放；
- Cancelled：按原 `RequestId` 调用 `StopByRequestId`；
- 不解析DivineBeasts/MOBA事实；
- 不把Gameplay Magnitude擅自映射成音量。

## 8. 性能设计

- 0 Tick / 0 Ticker；
- 不使用 `LoadSynchronous`；
- Definition和声音资产由租约异步加载；
- 活动声音由 `OnAudioFinishedNative` 事件回收；
- 同RequestId去重；
- 待加载硬上限128，平台实例追踪上限256，防止逻辑错误导致无界容器增长；
- 真正Voice竞争、虚拟化和抢占继续由SoundConcurrency/引擎音频设备决定；
- 具体平台Voice预算、CPU预算和内存预算必须实测，不在设计文档伪造数值。

## 9. 不在本轮范围

- Music Director（音乐导演）和跨地图持续音乐；
- Dialogue/Voice/字幕/本地化；
- 网络语音；
- 项目内容资产；
- 项目独有SFX C++子类；
- 自研混音器、DSP或音频线程。

## 10. 后续内容接入

DivineBeasts/内容包只需：

1. 创建 `UGamePlatformSFXDefinition` 实例并配置规范 `LogicalId`；
2. 在Presentation Catalog中把语义映射为 `ProviderChannel=SFX` 和该逻辑DefinitionId；
3. 配置Sound/MetaSound、Concurrency、Attenuation等真实资产；
4. 通过实际Client Cook与Review Map验证声音空间、遮挡、并发和可读性。
