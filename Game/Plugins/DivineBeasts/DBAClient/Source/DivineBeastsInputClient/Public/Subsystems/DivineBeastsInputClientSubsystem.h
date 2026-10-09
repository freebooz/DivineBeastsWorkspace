#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformInputTypes.h"
#include "Buffer/GamePlatformActionInputBuffer.h"
#include "UObject/PrimaryAssetId.h"
#include "DivineBeastsInputClientSubsystem.generated.h"

class IGamePlatformInputService;
class APlayerController;
class APawn;
class UGamePlatformAbilitySystemComponent;class UGamePlatformLocalHitstopSubsystem;
class USkeletalMeshComponent;

/** 神兽联盟项目输入事件；仍然是客户端请求，不代表服务器技能/移动已经成功。 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsGameplayInputEvent,
    const FGamePlatformInputEvent&);

/** 目标锁定请求；只表示本地玩家按下锁定语义，具体目标选择/服务器确认由Gameplay领域负责。 */
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsTargetLockRequestedNative,
    APawn*);

/**
 * UDivineBeastsInputClientSubsystem（神兽联盟输入客户端适配）。
 * 组合平台Input Service并完成神兽联盟LocalPlayer输入接线，不派生/复制平台LocalPlayerSubsystem。
 * 通用Move/Look继续消费平台语义；攻击/技能/锁定使用DivineBeasts.Input.*项目语义。
 */
UCLASS()
class DIVINEBEASTSINPUTCLIENT_API UDivineBeastsInputClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* Controller) override;

    /** 项目Gameplay事件广播；Character/Ability/UI消费方可按SemanticId继续分派。 */
    FDivineBeastsGameplayInputEvent& OnGameplayInput() { return GameplayInputEvent; }

    /** 目标锁定请求事件；项目目标系统订阅该事件，输入层不执行目标搜索或服务器权威选择。 */
    FDivineBeastsTargetLockRequestedNative& OnTargetLockRequested() { return TargetLockRequested; }

    /**
     * 启动一套项目Gameplay输入配置。Profile异步准备完成后自动申请指定Context并绑定当前本地Controller/Pawn的EnhancedInputComponent。
     * 同一LocalPlayer一次只允许一套活动Profile；调用本函数会先精确释放旧Profile拥有的上下文和绑定。
     */
    bool ActivateGameplayInput(
        const FPrimaryAssetId& ProfileId,
        const FString& LocalSettingsKey,
        const TArray<FName>& ContextNames,
        int32 ContextPriority,
        FGamePlatformResult& OutResult);

    /** 释放当前项目Profile及它拥有的Context/Binding；不影响其他输入系统持有的租约。 */
    FGamePlatformResult DeactivateGameplayInput();

    /** 当前Profile是否已由平台准备完成且已绑定真实EnhancedInputComponent。 */
    bool IsGameplayInputReady() const;

    /** 移动端项目按钮/摇杆使用稳定项目Tag申请Touch来源。 */
    FGamePlatformInputTouchHandle BeginProjectTouchInput(
        int32 PointerId,
        FGameplayTag SemanticTag,
        TWeakObjectPtr<UObject> Owner,
        FGamePlatformResult& OutResult);

    FGamePlatformResult UpdateProjectTouchInput(
        const FGamePlatformInputTouchHandle& Handle,
        const FInputActionValue& Value);

    FGamePlatformResult EndProjectTouchInput(
        const FGamePlatformInputTouchHandle& Handle);

private:
    void HandlePlatformInput(const FGamePlatformInputEvent& Event);
    /** 平台低频状态变化；回调栈只排队刷新，避免在平台广播期间重入输入服务修改。 */
    void HandlePlatformState(const FGamePlatformInputSnapshot& Snapshot);
    void QueueActivationRefresh();
    void TryFinalizeActivation();
    void RefreshControlledPawn();
    void HandleMoveLookInput(const FGamePlatformInputEvent& Event);
    void HandleProjectGameplayInput(const FGamePlatformInputEvent& Event);
    void ProcessAbilityInputOncePerFrame();
    void ClearAbilityInput();
    /** 视觉顿帧结束后只重放未过期、代次一致的动作输入，由GAS判断技能合法性。 */
    void HandleVisualHitstopFinished(USkeletalMeshComponent* RestoredMesh);
    void FlushBufferedAbilityInput();
    bool IsLocalPawnVisualHitstopActive() const;

    IGamePlatformInputService* PlatformInput = nullptr;
    FGamePlatformInputSubscription Subscription;
    FGamePlatformInputStateSubscription StateSubscription;
    FGamePlatformInputProfileHandle ProfileHandle;
    FGamePlatformInputBindingHandle BindingHandle;
    TArray<FGamePlatformInputContextHandle> ContextHandles;
    TArray<FName> RequestedContextNames;
    int32 RequestedContextPriority = 50;
    bool bActivationRefreshQueued = false;
    uint64 LastAbilityProcessFrame = MAX_uint64;
    /** 平台动作缓冲：本地输入的Started/End事件，不执行任何服务器权威行为。 */
    FGamePlatformActionInputBuffer BufferedAbilityInputs;
    TWeakObjectPtr<UGamePlatformLocalHitstopSubsystem> LocalHitstopSubsystem;
    FDelegateHandle VisualHitstopFinishedHandle;
    bool bReplayingBufferedAbilityInput = false;

    TWeakObjectPtr<APlayerController> CachedController;
    TWeakObjectPtr<APawn> CachedPawn;
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> CachedAbilitySystem;

    FDivineBeastsGameplayInputEvent GameplayInputEvent;
    FDivineBeastsTargetLockRequestedNative TargetLockRequested;
};
