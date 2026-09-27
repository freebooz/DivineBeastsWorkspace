# MVVMStrategy（MVVM策略）

当前平台没有启用 `ModelViewViewModel（UMG视图模型插件）`，因此 `UGamePlatformViewModelBase`不继承 `UMVVMViewModelBase`。

这是有意的兼容策略：先保持普通事件驱动接口稳定，再在 Editor/Client/Cook/性能均有证据后决定是否引入 UE5.8 MVVM。

无论采用 MVVM 或 Presenter（展示控制器），Widget 都不得到处直接读取 Gameplay Actor，更不得从 Widget 发起后端协议调用。

若未来启用 MVVM，应把变化限制在 ViewModel/Binding 层，并保留非 MVVM 路径作为可迁移方案。
