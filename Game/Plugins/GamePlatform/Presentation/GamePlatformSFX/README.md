# GamePlatformSFX（游戏平台音效插件）

版本：0.2.0｜2026-09-29

`GamePlatformSFX` 位于 `GamePlatform/Presentation（平台表现层）`，定位为**跨游戏、客户端专用的通用音效执行基础设施**。它不保存玩法事实、不拥有项目音频内容，也不实现第二套 Audio Mixer（音频混音器）。

当前正式链路：

```text
Gameplay / Application Fact
    → GamePlatformPresentation（中立表现请求）
    → ProviderChannel=SFX
    → GamePlatformSFXPresentationBridgeSubsystem（SFX表现桥）
    → IGamePlatformSFXService（音效服务）
    → UGamePlatformSFXWorldSubsystem（世界级音效执行器）
    → GamePlatformData Definition Lease（统一定义租约）
    → UGamePlatformSFXDefinition（音效定义）
    → UE AudioComponent / SoundBase / MetaSound
```

本轮审查后取消了原规格中“再建立一套 SFX Catalog/Resolver（音效目录/解析器）”的设计：语义解析已经由 `GamePlatformPresentation` 负责，Definition唯一身份和加载已经由 `GamePlatformData` 负责；SFX再次复制目录会形成双重真源。

当前已实现：

- `GamePlatformSFXClient — ClientOnly（仅客户端）`；
- `UGamePlatformSFXDefinition（平台音效定义）`，统一继承 `UGamePlatformDefinitionBase`；
- `IGamePlatformSFXService（音效服务）`；
- `UGamePlatformSFXWorldSubsystem（世界级执行器）`；
- 2D／世界位置／附着三种播放空间；
- `FGamePlatformSFXHandle（音效实例句柄）` 与 RequestId 去重/取消；
- `GamePlatformData` 的 `SFXRuntime` Bundle（音效运行分组）异步加载；
- SoundConcurrency（声音并发）与 Attenuation（衰减）原生策略接入；
- MetaSound/SoundCue 浮点参数白名单；
- Fade In / Fade Out（淡入淡出）；
- AudioFinished（播放结束）事件驱动资源释放；
- World/Owner 生命周期保护；
- 无 Tick/Ticker；
- Dedicated Server（专用服务器）隔离；
- `GamePlatformPresentation` 的 SFX Provider 接入。

明确不属于本插件：音乐导演、Dialogue/Voice（对白/语音）、字幕、本地化、服务器权威状态、音频设备枚举、用户设置真源、项目生肖/MOBA业务语义。

详细文档：

- `Docs/ImplementationSpecification.md`：最新实现规格；
- `Docs/Architecture.md`：架构、三层边界与调用链；
- `Docs/API.md`：Definition、Request、Service 使用方法；
- `Docs/PerformanceAndSecurity.md`：性能、并发、生命周期与安全边界；
- `Docs/TestingAndEvidence.md`：测试与真实验证证据；
- `Docs/ManualReview.md`：人工审核和组件清单；
- `Docs/审查整改方案与执行计划.md`：本轮审查结论、整改方案与执行计划。

2026-10-09 独立编译修复：音效桥接显式包含Engine/World.h，世界执行器显式包含SoundBase与SoundConcurrency的完整类型；SFXRuntime Bundle标识使用不可变const FName，避免运行时名称表类型被误写为constexpr。原完整Client构建的C2664/C2131复现日志位于Saved/Validation/FoundationM0/1d5d3f17-a598-4f01-ad58-9a094ffbde8e/Build/Client；后续构建结果单独记录，不改变声音或玩法行为。

本次完整Native构建证据：Client退出0见Saved/Validation/FoundationM0/0d516f9c-7d9c-4a49-b509-167c307aec0e/Build/Client；Server退出0见Saved/Validation/FoundationM0/8e56f639-5f43-4097-9d05-8ad14d337cbc/Build/Server。它们不代表Cook、真实WorldReady或功能体验验收通过。
