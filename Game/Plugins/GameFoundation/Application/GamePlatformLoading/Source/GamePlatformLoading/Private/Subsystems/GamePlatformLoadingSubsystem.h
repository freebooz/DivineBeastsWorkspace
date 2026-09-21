#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "Containers/Ticker.h"
#include "GamePlatformLoadingSubsystem.generated.h"
struct FGamePlatformLoadingScope;

/** 内部实例执行器；只公开服务门面，不公开注册表、调度器或租约集合。 */
UCLASS()
class UGamePlatformLoadingSubsystem final : public UGameInstanceSubsystem, public IGamePlatformLoadingService
{
    GENERATED_BODY()
public:
    UGamePlatformLoadingSubsystem();
    UGamePlatformLoadingSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformLoadingSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual FGamePlatformLoadingHandle StartLoadingOperation(const FGamePlatformLoadingOperationSpec& Spec, TWeakObjectPtr<UObject> Owner, FGamePlatformResult& OutResult) override;
    virtual FGamePlatformResult CancelLoadingOperation(const FGamePlatformLoadingHandle& Handle) override;
    virtual FGamePlatformResult ReleaseLoadingOperation(const FGamePlatformLoadingHandle& Handle) override;
    virtual FGamePlatformLoadingSnapshot GetLoadingSnapshot() const override;
    virtual FGamePlatformLoadingRegistration SubscribeLoadingState(const FGamePlatformLoadingHandle& Handle, TWeakObjectPtr<UObject> Owner, TFunction<void(const FGamePlatformLoadingSnapshot&)> Callback) override;
    virtual bool UnsubscribeLoadingState(const FGamePlatformLoadingRegistration& Registration) override;
    virtual FGamePlatformLoadingRegistration RegisterTaskFactory(FName Type, FGamePlatformLoadingTaskFactory Factory, FGamePlatformResult& OutResult) override;
    virtual FGamePlatformResult UnregisterTaskFactory(const FGamePlatformLoadingRegistration& Registration) override;
    virtual bool IsReadyToPlay(const FGamePlatformLoadingHandle& Handle) const override;
    virtual FGamePlatformResult ReportWorldOperable(const FGamePlatformLoadingHandle& Handle, UWorld& World) override;
private:
    bool Tick(float DeltaSeconds);
    void ReleaseTasks();
    TUniquePtr<FGamePlatformLoadingScope> Scope;
    FTSTicker::FDelegateHandle TickerHandle;
};
