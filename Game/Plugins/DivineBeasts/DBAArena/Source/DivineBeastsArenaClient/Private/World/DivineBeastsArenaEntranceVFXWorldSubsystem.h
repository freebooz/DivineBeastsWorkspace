#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "GamePlatformPresentationTypes.h"
#include "DivineBeastsArenaEntranceVFXWorldSubsystem.generated.h"

class ULocalPlayer;
class UGameInstance;
class UDivineBeastsPresentationClientSubsystem;
class UGamePlatformPresentationClientSubsystem;

/** 每客户端World一份入口装饰协调：借PrimaryLocalPlayer的既有内容包/中立表现服务；不拥有Niagara或Data加载器。 */
UCLASS()
class UDivineBeastsArenaEntranceVFXWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;

private:
    friend struct FDivineBeastsArenaEntranceVFXTestAccess;
    struct FBinding
    {
        FGamePlatformPresentationRequest Request;
        TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> Adapter;
        bool bSubmitting=false,bFailed=false;
    };
    TMap<TWeakObjectPtr<AActor>,FBinding> Bindings;
    TWeakObjectPtr<UGameInstance> GameInstance;
    TWeakObjectPtr<ULocalPlayer> PrimaryPlayer;
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> Presentation;
    TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> PlatformPresentation;
    FDivineBeastsPresentationContentPackHandle PackHandle;
    FDelegateHandle SpawnHandle,PlayerAddedHandle,PlayerRemovedHandle,PackChangedHandle;
    FGuid OwnerToken;
    bool bStarted=false,bClosing=false,bPackReady=false,bActivationAttempted=false;

    static FGameplayTag GetVortexSemantic();
    static FDivineBeastsPresentationContentPackFragment BuildContentFragment();
    void BindGameInstanceEvents();
    void RefreshOwner();
    /** 失败只保留无视觉结果；同一拥有者不因重复Actor事件循环重试。新World/拥有者可重新申请。 */
    void EnsureContentPack();
    void ReleasePresentation();
    void ObserveActor(AActor* Actor);
    void RemoveActor(AActor* Actor);
    void SubmitActor(AActor* Actor);
    void SubmitAll();
    static void CancelRequest(FBinding Binding);
    void HandlePack(const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State,const FString& Error,const FGuid& ExpectedOwnerToken);
    UFUNCTION() void HandleActorEndPlay(AActor* Actor,EEndPlayReason::Type Reason);
};
