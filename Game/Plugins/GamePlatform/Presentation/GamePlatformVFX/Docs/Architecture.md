# Architecture（架构）

GamePlatformVFX（游戏平台视觉特效插件）位于 GameFoundation（游戏平台基础层），只提供跨游戏通用 VFX 机制。

正式代码模块只有：
- GamePlatformVFXClient（VFX客户端运行模块，ClientOnly）：Definition/Catalog/Resolver、Presentation Provider、World生命周期、GamePlatformData预加载Lease、Niagara执行、实例/Handle、原生Pool桥接、Effect Type伸缩、Composite、诊断与自动化。
- GamePlatformVFXEditor（VFX编辑器模块，Editor）：Definition/Catalog/Dependency/Composite、Parameter Schema、Effect Type、LWC、Fixed Bounds等编辑器验证。

禁止创建GamePlatformVFXRuntime/GamePlatformVFXServer空模块。Dedicated Server Target显式禁用GamePlatformVFX。

正式调用链：Gameplay Fact / GameplayCue / Application Fact → GamePlatformPresentation Semantic Request → ProviderChannel=VFX → UGamePlatformVFXPresentationBridgeSubsystem → FGamePlatformVFXPresentationProvider → IGamePlatformVFXService → UGamePlatformVFXWorldSubsystem → Catalog/Resolver → Definition → GamePlatformData FGamePlatformAssetLoader/Lease → Niagara Executor → Instance Registry/Handle。

UGamePlatformVFXWorldSubsystem保持Private，低层Service只供客户端组合、工具、测试和精确Definition装配/预加载。Gameplay正常路径不得直接链接VFX Client。

World是运行实例、Catalog、Lease、Dedupe的隔离边界；Handle同时携带Generation与弱World身份。Catalog Resolver使用revision-aware cache并在同级歧义时Fail Closed。
