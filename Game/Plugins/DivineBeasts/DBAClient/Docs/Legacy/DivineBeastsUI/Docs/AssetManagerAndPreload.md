# AssetManagerAndPreload（资产管理与预加载）

Screen（页面）打开统一通过 UGamePlatformUIManagerSubsystem.OpenScreenAsync（平台异步页面打开）。

平台Screen Definition使用：

- TSoftClassPtr WidgetClass（软类引用）
- PreloadAssets（预加载软资产）
- GamePlatformData FGamePlatformAssetLoader（平台资产加载器）
- ActiveScreenLeases（活跃页面租约）
- CancelOpen（取消打开）
- CloseScreen（关闭页面释放）

项目层不创建DivineBeastsAssetManager，也不同步加载大型Widget。

当前FDivineBeastsUIScreenCatalog只有预期Widget/Definition路径，真实 .uasset 数量为0，所以真实页面异步加载、保留租约和关闭释放的UE运行证据均为“未执行”。
