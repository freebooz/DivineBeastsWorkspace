# AssetManagerCook（资产管理与Cook）

VFX Definition 现在属于统一 `GamePlatformDefinition` Primary Asset 类型。Niagara System、EffectType、平台/质量变体和辅助资源使用软引用，并通过 `VFXRuntime` Asset Bundle 由 `IGamePlatformDataService` 的 Definition Lease 持有。

旧 VFX Catalog 仅兼容低层工具，不再决定标准 Gameplay 的 Definition Cook 图。项目正式内容应由 Presentation Catalog 的 DefinitionId 和 GamePlatformData AssetRegistry/Bundle 关系进入客户端 Cook。

Client Cook 应包含 `GamePlatformVFXClient` 与实际被项目 Definition 引用的 VFX 内容；Review/Examples/TestAssets 必须从 Shipping 剥离。Dedicated Server 不应链接 `GamePlatformVFXClient`，Server Cook/Stage 不得包含纯 Niagara VFX 内容。

只有真实非空 Cook/Stage 工件通过客户端正向收据、AssetRegistry 和服务器泄漏审计后才能标记 Cook 通过。当前真实 Cook 证据仍未执行。