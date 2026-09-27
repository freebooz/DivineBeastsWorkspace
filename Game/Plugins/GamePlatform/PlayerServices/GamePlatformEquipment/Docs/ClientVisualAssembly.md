# ClientVisualAssembly（客户端视觉装配）

EquipmentClient 监听 Public Equipment Snapshot，只根据服务器批准的 VisualDefinitionId 装配表现。

第一版真实实现 StaticMesh（静态网格）路径。软资源通过 `FGamePlatformAssetLoader（平台资产加载器）`统一异步加载，并保存 FStreamableHandle（流式加载句柄）用于取消旧请求。

VisualRequestGeneration 与 AvatarGeneration 双重防止旧异步回调污染新角色。视觉失败不改变服务器 Gameplay 装备状态。