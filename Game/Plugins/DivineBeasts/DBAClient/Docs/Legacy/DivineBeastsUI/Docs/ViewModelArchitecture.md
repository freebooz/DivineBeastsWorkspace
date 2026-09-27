# ViewModelArchitecture（视图模型架构）

UDivineBeastsUIViewModel（项目通用视图模型）继承 UGamePlatformViewModelBase（平台视图模型基类）。

当前平台没有强制采用UE5.8 Beta UMG MVVM（测试阶段视图模型），因此本轮继续使用平台事件驱动ViewModel，不为本插件迁移到Beta框架。

每个ViewModel具有：

- Revision（修订号）
- PageGeneration（页面代次）
- Active生命周期
- Query Source状态订阅
- Command提交
- Pending Request集合
- 页面关闭时取消/解绑
- IsCallbackCurrent（回调是否仍有效）旧回调保护

统一页面状态支持 Loading / Ready / Empty / Disabled / Error / Retry / NoPermission / Submitting（加载/就绪/空/禁用/错误/重试/无权限/提交中）。

不使用原始Binding热路径或每Tick读取Gameplay Actor。
