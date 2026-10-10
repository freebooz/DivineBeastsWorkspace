// UIManager内部本地玩家主题事务服务；不另建Subsystem，不拥有StreamableManager。
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Styling/GamePlatformUIThemeTypes.h"
#include "Styling/GamePlatformUIThemeRequestState.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformUIThemeService.generated.h"

class UGameInstance;
class UGamePlatformUIManagerSubsystem;
class UGamePlatformUIThemeDefinition;
class IGamePlatformDataService;

/** 内部服务仅游戏线程调用；完整成功前保留上一主题，弱回调和GUID过滤迟到结果。 */
UCLASS()
class UGamePlatformUIThemeService final : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(UGamePlatformUIManagerSubsystem* InOwner);
    void Shutdown();
    FGuid RequestThemeAsync(const FPrimaryAssetId& ThemeId, const FGamePlatformUIThemeContext& Context);
    bool CancelThemeRequest(const FGuid& RequestId);
    FPrimaryAssetId GetCurrentThemeId() const;
    int64 GetRevision() const { return Revision; }
    UClass* ResolveStyle(EGamePlatformUIStyleKind Kind, FName Key, bool bFallback, FText& Error) const;
private:
    IGamePlatformDataService* GetDataService() const;
    void HandleLoaded(FGuid RequestId, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void FailPending(FGuid RequestId, const FText& Reason);
    void NotifyFinished(FGuid RequestId, bool bSuccess, const FText& Reason);
    void ReleaseLease(FGamePlatformDataLease Lease);
    TWeakObjectPtr<UGamePlatformUIManagerSubsystem> Owner;
    /** 仅弱引用资源所属实例；外部主题事件关闭Manager后仍可释放局部旧租约。 */
    TWeakObjectPtr<UGameInstance> DataInstance;
    FGamePlatformUIThemeRequestState RequestState;
    FGamePlatformDataLease PendingLease;
    FGamePlatformDataLease ActiveLease;
    FPrimaryAssetId PendingThemeId;
    FGamePlatformUIThemeContext PendingContext;
    FGamePlatformUIThemeContext ActiveContext;
    /** 强引用当前只读主题对象；真正资源生命期仍以Data租约为准。 */
    UPROPERTY(Transient) TObjectPtr<UGamePlatformUIThemeDefinition> ActiveDefinition = nullptr;
    int64 Revision = 0;
    /** 同步发布期间拒绝新请求，避免外部委托重入导致半主题状态；关停仍可调用。 */
    bool bPublishing = false;
};
