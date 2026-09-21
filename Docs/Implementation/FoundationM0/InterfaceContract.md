# Foundation M0 实际接口清单

日期：2026-09-21。仅记录已经写入的源码签名；头文件存在不表示UE已编译。Core与Data公开契约已写入，UE适配仍受正式工程构建阻断。

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

基线 `FGamePlatformFlowDefinition` 是普通C++配置、直接持有节点实例，不是资产。新增`UGamePlatformFlowDefinition`与它并存，不改名迁移旧配置；旧Configure保持DAG语义。用户已确认保留旧接口兼容扩展，不能新增第二个主执行器。

后续每阶段写入实际签名、所有权、错误与调用者；本文件不先声明未实现接口。

## Core已实现契约

公开头位于`GamePlatformCore/Public/Types/`。调用者包括Data主资产、定义验证及主工程`DBAFoundationCoordinatorData.cpp`，不是仅打印固定成功。

- `FGamePlatformId::{IsValid() const, ToString() const, static TryParse(const FString&, FGamePlatformId&)}`；字段Namespace、Name、LogicalVersion。格式`namespace.name@version`，ASCII小写规范化，逻辑版本1..INT32_MAX，单段最多64字符，总长192；失败清空输出；相等与`GetTypeHash(const FGamePlatformId&)`一致。没有ToName接口。
- `FGamePlatformVersion::{IsValid() const, ToString() const, static TryParse(const FString&, FGamePlatformVersion&), Compare(const FGamePlatformVersion&) const}`；三段非负数值，不把它混作身份逻辑版本或数据兼容策略。
- `FGamePlatformResult`默认NotExecuted；`Success()`、`Failure(FName,FString)`、`Cancelled(FString)`、`Unsupported(FName,FString)`、`IsSuccess() const`。显式区分未执行、成功、失败、取消、不支持，Code/Message为值诊断。

## Data已写入公开契约

入口`Public/Interfaces/IGamePlatformDataService.h`；私有子系统头不得被主工程或Flow包含。所有调用游戏线程，无网络权威含义。

```cpp
static IGamePlatformDataService* Get(UGameInstance& GameInstance);
FGamePlatformDataLease AcquireDefinition(const FPrimaryAssetId& DefinitionId,
    TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass, const TArray<FName>& Bundles,
    EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
    FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult);
const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease& Lease) const;
FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease& Lease);
EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const;
FGamePlatformDataDiagnostics GetDiagnostics() const;
// FGamePlatformDataCompletion = TFunction<void(const FGamePlatformDataLease&, const FGamePlatformResult&)>
```

`Lease`包含ScopeId、LeaseId、Generation、DefinitionId、Bundles、RequestState；后者是取得时快照，实时状态须查询服务。成功回调与持有期分离，成功后到Release前才可读取；重复合法本作用域释放幂等，伪造/跨实例拒绝。通知延后到游戏线程，弱调用者失效时抑制回调。World租约切图清理，Instance租约跨图保留。

`UGamePlatformDefinitionBase`继承`UGamePlatformPrimaryDataAsset`；逻辑身份映射为`GamePlatformDefinition:<规范身份>`，不使用文件名。`DataVersion.SchemaVersion/ContentRevision`与类CDO的可读结构范围分开。`RequiredDefinitions`是真实递归加载的依赖，不是未实现字段。进程唯一管理器配置路径`/Script/GamePlatformData.GamePlatformAssetManager`。

主工程`UDBAFoundationProbeDefinition::ProbeValue`是实际资产整数值；协同节点申请成功后交给协调器保有，HUD只读复制值，取消/重试/关闭释放。

## Flow资产兼容扩展（已写入源码，UE未验证）

仍由公开`UGamePlatformApplicationFlowSubsystem`提供类型化实例服务；主工程不包含插件Private头，也不使用通用对象查找器。

```cpp
using FGamePlatformFlowNodeFactory = TFunction<UGamePlatformFlowNode*(UGameInstance&)>;
FGamePlatformFlowFactoryHandle RegisterNodeFactory(FName ExecutorId,
    FGamePlatformFlowNodeFactory Factory, FGamePlatformResult& OutResult);
bool UnregisterNodeFactory(const FGamePlatformFlowFactoryHandle& Handle,
    FGamePlatformResult& OutResult);
FGamePlatformFlowHandle StartFlow(FGamePlatformDataLease& InOutReadyLease,
    UObject* Payload, FGamePlatformResult& OutResult);
bool CancelFlow(const FGamePlatformFlowNodeToken& Token, FGamePlatformResult& OutResult);
bool SubmitEvent(const FGamePlatformFlowNodeToken& Token,
    FGamePlatformFlowNodeResult Event, FGamePlatformResult& OutResult);
```

- 注册句柄含ScopeId、RegistrationId、ExecutorId，重复键拒绝。工厂只在启动预检通过后调用，每run创建本实例独立节点，不返回CDO或复用旧节点。活动流程期间不能改变工厂注册。
- StartFlow要求**本实例Data服务已就绪且实际类型正确**的流程租约。失败不消费输入；成功转移使用权并将输入清空，调用者不得继续用副本释放。流程终态/关闭先结束节点再释放该租约。不是复制一个裸定义指针后立刻释放租约。
- `FGamePlatformFlowContext`保留旧字段，末尾增加`uint64 NodeGeneration`及`FPrimaryAssetId InputDefinitionId`。`FGamePlatformFlowNodeToken`含Handle、NodeId、NodeGeneration；事件进入同一邮箱，重复/旧节点/旧运行/跨实例事件失败。
- CancelFlow精确匹配当前已进入节点；首节点尚未激活时使用保留的`Cancel(const FGamePlatformFlowHandle&)`取消运行。取消不伪装成成功终态。
- 快照新增NodeGeneration，旧字段与GetSnapshot保持。旧Configure/Start的DAG合同保留，不因新增资产循环选项静默放开。终态保留值快照，不保留可读的已释放定义指针。

`UGamePlatformFlowDefinition : UGamePlatformDefinitionBase`在`Public/Definitions/GamePlatformFlowDefinition.h`，字段EntryNodeId、Nodes、bAllowCycles、MaxImmediateCycleTransitions。节点字段NodeId、ExecutorId、InputDefinitionId、TimeoutSeconds、NextNodeId、Routes。图内没有项目节点类/地图硬引用；失败与取消由执行器进入相应终态，不是虚假项目业务节点。

## 主工程实际调用与所有权

`UDBAGameInstance`持有协调器；`Init/Shutdown`装配/清理，`FoundationCancel/FoundationRetry`为显式开发命令。协调器先等待本实例世界/本地观察者和注册表发现，注册五个中立键，然后通过Data申请`foundation.flow@1`并传给StartFlow。项目`UDBAFoundationNode`读取Context.InputDefinitionId，真实申请`foundation.probe@1`，完成后将租约转给协调器。EnterSandbox安装监听后调用OpenLevel，URL携带独占操作GUID，回调匹配实例、目标包和操作，再等待HasBegunPlay；Ready还要求真实本地Pawn及探针可读。

项目只读诊断每次复制值，地图变化后重新为当前本地控制器建立HUD，不持有旧世界HUD指针。关闭顺序为申请代次失效→取消运行/节点清理→释放自身剩余租约→撤销工厂/监听→移除实例轮询。专用服务器只记录当前世界日志；Commandlet不启动玩家流程。

准确签名以对应公开头及本文件同步维护；底层引擎接口已按本机源核对，但本文件没有宣称反射编译、真实资产或UE运行通过。
