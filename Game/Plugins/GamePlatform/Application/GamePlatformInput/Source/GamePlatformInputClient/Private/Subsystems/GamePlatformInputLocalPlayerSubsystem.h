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
    virtual FGamePlatformInputStateSubscription SubscribeInputState(TWeakObjectPtr<UObject>,TFunction<void(const FGamePlatformInputSnapshot&)>) override;
    virtual bool UnsubscribeInputState(const FGamePlatformInputStateSubscription&) override;
    virtual FGamePlatformInputSnapshot GetInputSnapshot() const override;
    virtual FGamePlatformInputDiagnostics GetInputDiagnostics() const override;
    virtual FGamePlatformResult NotifyInputDeviceActivity(EGamePlatformInputDeviceFamily) override;
    virtual FGamePlatformResult SetAccessibilitySettings(const FGamePlatformInputAccessibilitySettings&) override;
    virtual FGamePlatformInputAccessibilitySettings GetAccessibilitySettings() const override;
    virtual TArray<FGamePlatformInputMapping> ListPlayerMappings() const override;
    virtual FGamePlatformInputRebindPreview PreviewRebind(FName,int32,FKey) const override;
    virtual FGamePlatformResult ApplyRebind(FName,int32,FKey) override;
    virtual FGamePlatformResult ResetMappings(FName) override;
    virtual FGamePlatformResult SaveInputPreferences() override;
    virtual FGamePlatformInputTouchHandle BeginTouchInput(int32,EGamePlatformInputSemantic,TWeakObjectPtr<UObject>,FGamePlatformResult&) override;
    virtual FGamePlatformInputTouchHandle BeginTouchInputBySemantic(int32,FGamePlatformInputSemanticId,TWeakObjectPtr<UObject>,FGamePlatformResult&) override;
    virtual FGamePlatformResult UpdateTouchInput(const FGamePlatformInputTouchHandle&,const FInputActionValue&) override;
    virtual FGamePlatformResult EndTouchInput(const FGamePlatformInputTouchHandle&) override;
private:
    /**
     * 更新当前设备族并维护修订号/诊断计数；相同设备重复上报为O(1)无操作。
     * bCountAsActivity=false仅用于初始化，避免把初始平台默认值计入“用户设备切换”。
     */
    void SetActiveDeviceFamily(EGamePlatformInputDeviceFamily DeviceFamily,bool bCountAsActivity);
    /** 统一Touch签发入口；Slot仅来自已编译Profile，运行时保持数组O(1)访问。 */
    FGamePlatformInputTouchHandle BeginTouchInputBySlot(int32 PointerId,int32 Slot,TWeakObjectPtr<UObject> Owner,FGamePlatformResult& OutResult);
    bool Tick(float DeltaSeconds);
    /** 仅在存在弱Owner租约时安排低频维护Ticker；空闲LocalPlayer不产生固定轮询。 */
    void ScheduleMaintenance();
    void CompleteProfile(uint64 Generation,const FGamePlatformResult& Result);
    bool CanMutate() const;
    bool IsCurrentOwner(TWeakObjectPtr<UObject> Owner) const;
    void Interrupt(uint8 Channels,EGamePlatformInputEndReason Reason);
    void Publish(FGamePlatformInputEvent Event);
    /** 只在公开快照真实变化时广播低频状态；广播期间禁止结构性修改。 */
    void PublishState(bool bForce = false);
    void Route(const FInputActionValue& Value,int32 Slot,ETriggerEvent Phase,uint64 BindingGeneration);
    /** Enhanced Input绑定回调；高频路径直接携带编译期Slot，不做GameplayTag/TMap查找。 */
    void HandleBoundInput(const FInputActionValue& Value,int32 Slot,ETriggerEvent Phase,uint64 BindingGeneration);
    void RebuildMappings();
    FGamePlatformResult PreparePreferences();
    void ReleasePreferences();
    UFUNCTION() void OnMappingsRebuilt();
    UFUNCTION() void OnContextAdded(const UInputMappingContext* Context);
    UFUNCTION() void OnContextRemoved(const UInputMappingContext* Context);
    TUniquePtr<FGamePlatformInputScope> Scope;
    FTSTicker::FDelegateHandle TickerHandle;
};
