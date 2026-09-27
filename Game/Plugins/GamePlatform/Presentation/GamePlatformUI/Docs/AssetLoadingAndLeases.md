# AssetLoadingAndLeases（资产加载与租约）

页面 WidgetClass（控件类）和 PreloadAssets（预加载资产）都使用 Soft Reference（软引用）。`UGamePlatformUIManagerSubsystem`不直接调用第二套 AssetManager，而是通过 `GamePlatformData`公开的 `FGamePlatformAssetLoader（平台资产加载器）`提交异步加载。

每个打开请求保存 `FStreamableHandle（流式加载句柄）`。页面成功创建后，该 Handle 转移到 ActiveScreenLeases（活动页面租约）；页面反激活时释放。取消请求会取消对应 Handle。

加载回调首先检查 RequestId 仍存在、Definition 仍注册、Root Layout 仍有效以及标签条件仍满足；任何一项失效都不创建页面。

当前 `GamePlatformData`仅补了中立加载薄封装，尚未建立完整 Primary Asset（主资产）版本、租约统计和统一诊断体系。
