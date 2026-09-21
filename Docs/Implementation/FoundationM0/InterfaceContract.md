# Foundation M0 实际接口清单

日期：2026-09-21。仅记录已经存在的源码签名；拟实现能力不得冒充可调用接口。Core／Data 尚无公开接口。

## 现有 ApplicationFlow 兼容基线

公开头：`Game/Plugins/GameFoundation/Application/GamePlatformApplicationFlow/Source/GamePlatformApplicationFlow/Public/API/GamePlatformApplicationFlowSubsystem.h`。

```cpp
bool Configure(const FGamePlatformFlowDefinition& Definition, FString& OutError);
FGamePlatformFlowHandle Start(UObject* Payload, FString& OutError);
bool Cancel(const FGamePlatformFlowHandle& Handle);
FGamePlatformFlowSnapshot GetSnapshot() const;
FGamePlatformFlowFinished& OnFinished();
```

游戏线程调用；Configure为原子替换，失败保留旧配置。Payload可空，非空必须属于本GameInstance。句柄含ScopeId与RunId。终态快照保留至下一次配置／启动。拒绝运行中重配置、旧句柄、跨实例句柄及重入控制。公开节点契约：

```cpp
virtual void Execute(const FGamePlatformFlowContext& Context,
                     FGamePlatformFlowCompletion Complete);
virtual void Finish(EGamePlatformFlowFinishReason Reason);
```

Execute／Finish在游戏线程，Complete可从工作线程调用，下一次游戏线程调度处理。Finish清理每次尝试持有的资源。当前调用者是回调节点适配与模块测试；正式主工程基线无调用者。

## 与M0的差异

当前 `FGamePlatformFlowDefinition` 是普通C++配置、直接持有节点实例，不是 `UGamePlatformFlowDefinition` 资产；当前没有RegisterNodeFactory／StartFlow／SubmitEvent／CancelFlow公开入口，没有数据租约。用户已确认保留旧接口兼容扩展，不能新增第二个主执行器或直接重命名旧类。

后续每阶段写入实际签名、所有权、错误与调用者；本文件不先声明未实现接口。
