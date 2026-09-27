# API（公开接口）

GamePlatformVFX（游戏平台视觉特效插件）的低层公开契约是 IGamePlatformVFXService（平台VFX服务接口），仅用于客户端组合、工具、自动化测试以及已知精确 DefinitionId（定义编号）的装配/预加载。

正式Gameplay（玩法）正常路径为 GamePlatformPresentation（平台表现）中立请求 → ProviderChannel=VFX → UGamePlatformVFXPresentationBridgeSubsystem（VFX表现桥子系统）→ FGamePlatformVFXPresentationProvider（VFX表现提供者）→ WorldSubsystem（世界子系统）。

Service提供 Play、Stop、IsActive、Preload、CancelPreload、RegisterCatalog/UnregisterCatalog。Request只允许逻辑DefinitionId、SemanticTag、上下文、受Schema约束参数和表现预测信息，不接受任意资产路径或类路径。

新增Go业务后端接口：不适用。新增Go微服务：不适用。