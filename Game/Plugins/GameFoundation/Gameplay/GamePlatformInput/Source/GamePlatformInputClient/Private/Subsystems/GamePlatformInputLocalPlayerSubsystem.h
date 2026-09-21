#pragma once
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Interfaces/IGamePlatformInputService.h"
#include "Containers/Ticker.h"
#include "GamePlatformInputLocalPlayerSubsystem.generated.h"
class UInputMappingContext;
struct FGamePlatformInputScope;

/** 私有本地玩家门面，不公开可变原生注册表；所有工作限定游戏线程。 */
UCLASS(Transient)
class UGamePlatformInputLocalPlayerSubsystem final : public ULocalPlayerSubsystem, public IGamePlatformInputService
{
    GENERATED_BODY()
public:
    UGamePlatformInputLocalPlayerSubsystem();
    UGamePlatformInputLocalPlayerSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformInputLocalPlayerSubsystem() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* Controller) override;
    virtual FGamePlatformInputProfileHandle PrepareInputProfile(const FPrimaryAssetId&,const FString&,FGamePlatformResult&) override;
    virtual FGamePlatformResult ReleaseInputProfile(const FGamePlatformInputProfileHandle&) override;
    virtual FGamePlatformInputContextHandle AcquireInputContext(FName,int32,TWeakObjectPtr<UObject>,FGamePlatformResult&) override;
    virtual FGamePlatformResult ReleaseInputContext(const FGamePlatformInputContextHandle&) override;
    virtual FGamePlatformInputBindingHandle BindInputReceiver(UEnhancedInputComponent&,FGamePlatformResult&) override;
    virtual FGamePlatformResult UnbindInputReceiver(const FGamePlatformInputBindingHandle&) override;
    virtual FGamePlatformInputBlockHandle AcquireInputBlock(uint8,FName,TWeakObjectPtr<UObject>,FGamePlatformResult&) override;
    virtual FGamePlatformResult ReleaseInputBlock(const FGamePlatformInputBlockHandle&) override;
    virtual void SetApplicationFocus(bool) override;
    virtual FGamePlatformInputSubscription SubscribeInputEvents(TWeakObjectPtr<UObject>,TFunction<void(const FGamePlatformInputEvent&)>) override;
    virtual bool UnsubscribeInputEvents(const FGamePlatformInputSubscription&) override;
    virtual FGamePlatformInputSnapshot GetInputSnapshot() const override;
    virtual TArray<FGamePlatformInputMapping> ListPlayerMappings() const override;
    virtual FGamePlatformInputRebindPreview PreviewRebind(FName,int32,FKey) const override;
    virtual FGamePlatformResult ApplyRebind(FName,int32,FKey) override;
    virtual FGamePlatformResult ResetMappings(FName) override;
    virtual FGamePlatformResult SaveInputPreferences() override;
    virtual FGamePlatformInputTouchHandle BeginTouchInput(int32,EGamePlatformInputSemantic,TWeakObjectPtr<UObject>,FGamePlatformResult&) override;
    virtual FGamePlatformResult UpdateTouchInput(const FGamePlatformInputTouchHandle&,const FInputActionValue&) override;
    virtual FGamePlatformResult EndTouchInput(const FGamePlatformInputTouchHandle&) override;
private:
    bool Tick(float DeltaSeconds);
    void CompleteProfile(uint64 Generation,const FGamePlatformResult& Result);
    bool CanMutate() const;
    bool IsCurrentOwner(TWeakObjectPtr<UObject> Owner) const;
    void Interrupt(uint8 Channels,EGamePlatformInputEndReason Reason);
    void Publish(FGamePlatformInputEvent Event);
    void Route(const FInputActionValue& Value,EGamePlatformInputSemantic Semantic,ETriggerEvent Phase,uint64 BindingGeneration);
    void RebuildMappings();
    FGamePlatformResult PreparePreferences();
    void ReleasePreferences();
    UFUNCTION() void OnMappingsRebuilt();
    UFUNCTION() void OnContextAdded(const UInputMappingContext* Context);
    UFUNCTION() void OnContextRemoved(const UInputMappingContext* Context);
    TUniquePtr<FGamePlatformInputScope> Scope;
    FTSTicker::FDelegateHandle TickerHandle;
};
