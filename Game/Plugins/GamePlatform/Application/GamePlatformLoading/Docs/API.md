# 公开API（依据实际头文件）

头文件为`Public/Interfaces/IGamePlatformLoadingService.h`、`IGamePlatformLoadingTask.h`、`Public/Types/GamePlatformLoadingTypes.h`。本轮没有蓝图节点或反射配置资产；私有子系统类型不作为公开接口。

下列方法均只允许游戏线程，服务属于传入GameInstance。实例销毁后不得保留裸服务指针；每次从有效实例重新取得。所有者是弱UObject，必须是本实例或Outer链属于本实例；销毁后抑制外部回调并清理。

```cpp
static IGamePlatformLoadingService* Get(UGameInstance& Instance);
FGamePlatformLoadingHandle StartLoadingOperation(const FGamePlatformLoadingOperationSpec& Spec,
    TWeakObjectPtr<UObject> Owner, FGamePlatformResult& OutResult);
FGamePlatformResult CancelLoadingOperation(const FGamePlatformLoadingHandle& Handle);
FGamePlatformResult ReleaseLoadingOperation(const FGamePlatformLoadingHandle& Handle);
FGamePlatformLoadingSnapshot GetLoadingSnapshot() const;
FGamePlatformLoadingRegistration SubscribeLoadingState(const FGamePlatformLoadingHandle& Handle,
    TWeakObjectPtr<UObject> Owner, TFunction<void(const FGamePlatformLoadingSnapshot&)> Callback);
bool UnsubscribeLoadingState(const FGamePlatformLoadingRegistration& Registration);
FGamePlatformLoadingRegistration RegisterTaskFactory(FName Type,
    FGamePlatformLoadingTaskFactory Factory, FGamePlatformResult& OutResult);
FGamePlatformResult UnregisterTaskFactory(const FGamePlatformLoadingRegistration& Registration);
bool IsReadyToPlay(const FGamePlatformLoadingHandle& Handle) const;
FGamePlatformResult ReportWorldOperable(const FGamePlatformLoadingHandle& Handle, UWorld& World);
```

| 方法 | 输入/输出、重复调用与失败含义 |
| --- | --- |
| Get | 未初始化或不存在返回nullptr；不自动启动操作 |
| Start | 同步校验完整任务图；失败句柄无效、OutResult失败；接纳不代表已完成，下一轮才启动任务；已有未释放操作返回Busy |
| Cancel | 完整句柄匹配才生效；运行中取消并清理，不回滚后端事务；匹配的已终态操作重复成功，不把Ready重写成Cancelled |
| Release | 运行中先取消；Ready时显式归还自身资源；匹配句柄重复释放成功，旧操作/跨实例失败 |
| Snapshot | 最近操作值副本；包含单调时间、状态、任务错误与资源持有标记；不暴露世界指针或主资产对象 |
| Subscribe | 同步登记，后续调度通知；同轮进度可合并，每订阅至多通知一次终态；取消快照可跨下一操作发布但仍按原句柄过滤 |
| Unsubscribe | 精确注册身份删除；不存在/跨作用域false；重复订阅是独立登记，调用者分别撤销 |
| Register | 返回可撤销注册身份；重复/保留类型、空工厂、活动操作期间修改失败；不接受覆盖 |
| Unregister | 操作未释放返回Busy；已撤销或跨实例失败；新操作不能调用已撤销工厂 |
| Ready | 同时核对完整句柄、屏障终态、资源持有、所有者及所需世界事实；进度1不能替代 |
| World | 只接纳本操作当前世界、Game/PIE、已BeginPlay、未退出且包身份匹配的项目声明；错误为WorldScopeMismatch或WorldNotOperable |

主要错误码：`ServiceUnavailable`、`Busy`、`InvalidOwnerOrPurpose`、`TaskFactoryMissing`、`SessionPrerequisiteMissing`、`InvalidOperation`、`InvalidTask`、`DuplicateTask`、`UnknownDependency`、`DuplicateDependency`、`DependencyCycle`、`InvalidWeightSum`、`InvalidRequirement`、`WorldMustBeRequired`、`TargetWorldMissing`、`StaleOperation`、`ReentrantMutation`、`ForeignRegistration`、`StaleRegistration`。任务错误保留在TaskSnapshot.Error；总失败另有`TaskFailed`或`OperationTimeout`。错误不携带Token或个人资料。

## 字段与单位

- Handle：OwnerScopeId为实例作用域，OperationId为本次GUID，Generation为不可复用操作代次；修改任一字段均不能控制原操作。
- OperationSpec：Purpose为中立用途；TargetWorldPackage为调用方提供的目标长包名；TimeoutSeconds默认60秒，必须有限且大于0；Tasks启动后冻结，最多256项。
- TaskSpec：TaskId在操作内唯一且按FName语义不区分大小写；TaskType为已登记类型；Requiredness定义失败策略；Weight为正有限权重；TimeoutSeconds默认30秒；Dependencies为必须成功/降级成功的前置任务。
- DataRequest：DefinitionId为Data主资产身份；ExpectedClass为预期定义基类或派生类，默认平台定义基类；Bundles仅是本租约需求，不修改其他调用者分组。
- FallbackTaskType/FallbackData：仅用于降级任务的独立回退尝试，不能把失败标成成功；回退另有任务执行代次和超时，仍受操作总截止限制。
- Snapshot：State为操作状态，StartTimeSeconds/DeadlineSeconds来自单调时钟，OverallProgress01只用于显示；TaskSnapshots保留执行代次、回退标志及错误；bResourcesHeld为本操作是否仍占有资源期。

## 最小C++调用片段

```cpp
FGamePlatformLoadingOperationSpec Spec;
Spec.Purpose = TEXT("DefinitionPreload");
FGamePlatformLoadingTaskSpec Task;
Task.TaskId = TEXT("Rules");
Task.TaskType = TEXT("Data");
Task.Data.DefinitionId = DefinitionId;
Spec.Tasks.Add(Task);
FGamePlatformResult Accepted;
auto* Loading = IGamePlatformLoadingService::Get(Instance);
if (Loading)
{
    const auto Handle = Loading->StartLoadingOperation(Spec, &Instance, Accepted);
    // 调用方应保存Handle，订阅或读取快照；只在不再使用资源时Release。
}
```

完整取消与Flow接线见真实`DBALoadingFlowNode.cpp`。上述片段只是调用形状，不是能独立运行或自动释放的完整程序。
