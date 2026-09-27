# ViewModelArchitecture（视图模型架构）

`UGamePlatformViewModelBase`当前采用普通 UObject（对象）+事件驱动，不强制启用 `ModelViewViewModel/MVVM（模型-视图-视图模型）`插件。

Revision（修订号）在状态变化时递增；PageGeneration（页面代次）在页面开始/结束时更新。异步业务回调应携带期望值并调用 `IsCallbackCurrent`，从而忽略旧页面/旧请求结果。

ViewModel 不保存 Password/Token/Ticket（密码/令牌/票据）等敏感长期状态，不直接访问后端 HTTP，也不成为 Gameplay 真源。

项目可以在验证 UE5.8 MVVM 后派生/适配，但不能要求其它基础插件为 Beta MVVM ABI（应用二进制接口）承担强依赖。
