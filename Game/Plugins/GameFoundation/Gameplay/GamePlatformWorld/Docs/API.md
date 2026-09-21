# 实际公开接口与使用合同

准确声明位于Public/Interfaces/IGamePlatformWorldService.h。全部接口仅游戏线程；返回Core FGamePlatformResult，失败不等于异步完成；无服务Get返回nullptr。

```cpp
static IGamePlatformWorldService* Get(UWorld& World);
FGamePlatformResult InitializeDevelopment(const FPrimaryAssetId& DefinitionId,
    const FGamePlatformVersion& BuildVersion, FName ServerRole = NAME_None);
FGamePlatformResult InitializeSessionWorld();
FGamePlatformWorldReadinessSnapshot GetReadiness();
FGamePlatformWorldRegistration RegisterRegionProvider(
    const FGamePlatformRegionProvider&, FGamePlatformResult& OutResult);
bool UnregisterRegionProvider(const FGamePlatformWorldRegistration&);
FGamePlatformResult QueryRegion(const FVector& Position, FGamePlatformId& OutRegion);
FGamePlatformResult UpdateObserver(TWeakObjectPtr<UObject> Observer,
    const FVector& Position, const FGuid& Generation);
FGamePlatformWorldRegistration SubscribeRegions(TWeakObjectPtr<UObject> Owner,
    TFunction<void(const FGamePlatformRegionEvent&)> Callback);
bool Unregister(const FGamePlatformWorldRegistration&);
FGamePlatformWorldRegistration RegisterReadinessContributor(TWeakObjectPtr<UObject> Owner,
    TSharedRef<IGamePlatformWorldReadinessContributor>, FGamePlatformResult& OutResult);
FGamePlatformWorldStreamingHandle RequestStreaming(
    const FGamePlatformWorldStreamingRequest&, FGamePlatformResult& OutResult);
FGamePlatformResult CancelStreaming(const FGamePlatformWorldStreamingHandle&);
```

InitializeDevelopment只接纳一次；失败或已开始不覆盖现有上下文。离线例外要求FoundationWorld且非Shipping，不允许NM_Client或NM_ListenServer；专服角色仅OpenWorld/Village/MainArena。InitializeSessionWorld当前明确Unsupported，不接受外部自称已认证的Assignment。

注册返回世界代次+随机ID；撤销已不存在记录返回false，不产生副作用。QueryRegion先清空输出，区域外成功但无ID，等优先级等体积重叠返回AmbiguousRegion。坐标和边界单位厘米，必须有限且盒非退化。观察者必须属于本World，不假定第零号玩家。

订阅回调在后续世界采样发布；失效拥有者自动撤销。回调/贡献者Evaluate期间拒绝修改World登记表，调用方应在后续游戏线程调度操作。贡献者Evaluate返回NotExecuted等待、Success完成，其余错误使就绪失败。

Loading工厂：`TUniquePtr<IGamePlatformLoadingTask> GamePlatformWorldServices::CreateReadinessTask()`。Start绑定真实当前世界和代次；Poll传播失败；IsReadyToUse在成功后仍复核；Release只放弃任务绑定，不释放世界自己拥有的租约。

