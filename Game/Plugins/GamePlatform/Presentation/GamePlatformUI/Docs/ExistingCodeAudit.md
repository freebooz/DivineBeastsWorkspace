# ExistingCodeAudit（现有代码审计）

本次生产化开始前，正式 Owner（所有者）已经是 Game/Plugins/GamePlatform/Presentation/GamePlatformUI；没有发现真实 Frontend/Plugins/GamePlatformUIClient 旧插件，也没有第二个 UGamePlatformUIManagerSubsystem（平台UI管理子系统）定义。迁移决策是保留当前正式 Owner，禁止再建兼容空壳或第二套 UI Framework（界面框架）。

107～118 十二个对象当前审计如下。compile status（编译状态）中的“未执行”表示当前 Runner 没有 UE5.8 工具链，不能用源码存在代替 UBT/UHT（构建/反射）证据。

| 对象 | Actual Path（实际路径） | Reflection（反射） | Test（测试） | Compile（编译） | Migration Decision（迁移决策） |
| --- | --- | --- | --- | --- | --- |
| UGamePlatformUIManagerSubsystem（UI管理子系统） | Source/GamePlatformUIClient/Public/Manager/GamePlatformUIManagerSubsystem.h | UCLASS源码存在；UHT未执行 | 静态门禁通过；Registry/Travel自动化源码存在；运行未执行 | 未执行 | 保留正式Owner |
| UGamePlatformUIScreen（页面基类） | Public/Screens/GamePlatformUIScreen.h | UCLASS源码存在；UHT未执行 | CommonUI/Focus静态门禁通过；运行未执行 | 未执行 | 保留 |
| UGamePlatformUIScreenDefinition（页面定义） | Public/Definitions/GamePlatformUIScreenDefinition.h | UCLASS/UPrimaryDataAsset源码存在；UHT未执行 | Definition安全/Layer/Focus/路径/抽象类拒绝自动化源码已补；运行未执行 | 未执行 | 保留并生产化加固 |
| UGamePlatformHUDWidget（HUD基类） | Public/Screens/GamePlatformHUDWidget.h | UCLASS源码存在；UHT未执行 | Layer静态门禁通过；真实HUD资产/运行未执行 | 未执行 | 保留 |
| UGamePlatformDialogWidget（对话框基类） | Public/Dialogs/GamePlatformDialogWidget.h | UCLASS源码存在；UHT未执行 | 一次性Resolve源码规则存在；UE Automation未执行 | 未执行 | 保留 |
| UGamePlatformViewModelBase（视图模型基类） | Public/ViewModels/GamePlatformViewModelBase.h | UCLASS源码存在；UHT未执行 | Revision/PageGeneration自动化源码存在；运行未执行 | 未执行 | 保留非MVVM事件驱动方案 |
| UGamePlatformLoadingScreenService（加载界面服务） | Public/Loading/GamePlatformLoadingScreenService.h | UCLASS/USTRUCT源码存在；UHT未执行 | Token/Progress自动化源码已补；运行未执行 | 未执行 | 保留并增加进度归一化 |
| UGamePlatformUILayerStack（UI层级栈） | Public/Layers/GamePlatformUILayerStack.h | UCLASS源码存在；UHT未执行 | Layer/Travel静态门禁通过；Root Layout运行未执行 | 未执行 | 保留 |
| UGamePlatformToastWidget（短提示基类） | Public/Dialogs/GamePlatformToastWidget.h | UCLASS源码存在；UHT未执行 | Priority/Dedupe源码规则存在；真实运行未执行 | 未执行 | 保留并补优先级替换 |
| UGamePlatformMenuScreen（菜单页面基类） | Public/Screens/GamePlatformMenuScreen.h | UCLASS源码存在；UHT未执行 | CommonUI继承静态检查通过；运行未执行 | 未执行 | 保留 |
| UGamePlatformUIInputPolicy（UI输入策略） | Public/Input/GamePlatformUIInputPolicy.h | UCLASS源码存在；UHT未执行 | GameOnly/UIOnly/GameAndUI自动化源码存在；设备运行未执行 | 未执行 | 保留 |
| UGamePlatformUIRouteDefinition（UI路由定义） | Public/Routing/GamePlatformUIRouteDefinition.h | UCLASS/UPrimaryDataAsset源码存在；UHT未执行 | Target/Layer/Self/Cycle/BackRoute自动化源码已补；运行未执行 | 未执行 | 保留并生产化加固 |

CommonGameViewportClient（CommonUI视口客户端）当前已在 Game/Config/DefaultEngine.ini 配置。ModelViewViewModel/MVVM（模型-视图-视图模型）插件当前没有启用，UGamePlatformViewModelBase继续使用普通 UObject（对象）+ Delegate（委托）的事件驱动方案。

当前项目层 DivineBeastsUI（神兽联盟项目UI插件）已经存在，但只是上层消费者；GamePlatformUI没有反向依赖它。

