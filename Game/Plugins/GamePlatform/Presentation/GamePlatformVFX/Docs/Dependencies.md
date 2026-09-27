# Dependencies（依赖边界）

允许：Core、CoreUObject、Engine、GameplayTags、Niagara、DeveloperSettings，以及编辑器模块所需 UnrealEd/AssetRegistry。

禁止：MobaCommon（MOBA通用层）和 DivineBeasts（神兽联盟项目层）的类、模块和资产；禁止 Dedicated Server 依赖 GamePlatformVFXClient 或 GamePlatformVFXEditor。

当前两个外部集成边界：
1. GamePlatformData（平台数据插件）仍为骨架，因此 VFX 预加载暂用 UE AssetManager/StreamableManager；待统一数据加载 API 完成后切换到该 API。
2. GamePlatformPresentation（平台表现插件）仍为骨架，因此 VFX Provider 已实现，但正式 Provider 注册契约尚待该插件提供。

这两个边界均不通过在 VFX 插件中复制第二套平台框架来规避。
