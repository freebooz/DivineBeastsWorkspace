#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IGamePlatformPCGService.h"
#include "Containers/Ticker.h"
#include "GamePlatformPCGWorldSubsystem.generated.h"
class UPCGComponent;
struct FGamePlatformPCGWorldScope;

/** 原生PCG之上的请求所有权门面；不持有全球区域目录，不替代引擎执行器。 */
UCLASS()
class UGamePlatformPCGWorldSubsystem final : public UWorldSubsystem, public IGamePlatformPCGService
{
    GENERATED_BODY()
public:
    UGamePlatformPCGWorldSubsystem();
    UGamePlatformPCGWorldSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformPCGWorldSubsystem() override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual FGamePlatformResult ValidateProfile(const UGamePlatformPCGProfileDefinition& Profile) const override;
    virtual FGamePlatformPCGHandle RequestGeneration(const FGamePlatformPCGRequest& Request,FGamePlatformResult& OutResult) override;
    virtual FGamePlatformResult CancelGeneration(const FGamePlatformPCGHandle& Handle) override;
    virtual FGamePlatformResult ReleaseGeneration(const FGamePlatformPCGHandle& Handle) override;
    virtual FGamePlatformPCGSnapshot GetGenerationSnapshot(const FGamePlatformPCGHandle& Handle) const override;
    virtual FGamePlatformPCGSubscription SubscribeGeneration(const FGamePlatformPCGHandle& Handle,TWeakObjectPtr<UObject> Owner,TFunction<void(const FGamePlatformPCGSnapshot&)> Callback) override;
    virtual bool UnsubscribeGeneration(const FGamePlatformPCGSubscription& Subscription) override;
private:
    bool Tick(float DeltaSeconds);
    void OnNativeSignal(FGuid OperationId,TWeakObjectPtr<UPCGComponent> Component,bool bGenerated);
    void BeginCleanup(const FGuid& OperationId);
    void ProcessRequest(const FGuid& OperationId);
    TUniquePtr<FGamePlatformPCGWorldScope> Scope;
    FTSTicker::FDelegateHandle TickerHandle;
};
