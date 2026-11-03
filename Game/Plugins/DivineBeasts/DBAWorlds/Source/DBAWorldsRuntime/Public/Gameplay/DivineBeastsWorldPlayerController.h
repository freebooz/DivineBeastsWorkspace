#pragma once
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "Gameplay/DivineBeastsPlayerControlsComponent.h"
#include "DivineBeastsWorldPlayerController.generated.h"
class AGamePlatformPlayerStateBase;
class UGamePlatformExperienceComponent;
class AGameStateBase;
class UDivineBeastsCharacterComponent;
/** 世界准备/服务器资格适配器；本地操控唯一实现归PlayerControls组件。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsWorldPlayerController : public AGamePlatformPlayerControllerBase
{
    GENERATED_BODY()
public:
    ADivineBeastsWorldPlayerController();
    virtual int32 GetGameplayAvatarGeneration() const override;
    FSimpleMulticastDelegate OnLocalWorldFactsChanged;
    /** 本地ESC意图；UI客户端拥有订阅和菜单，共享世界模块不依赖客户端UI实现。 */
    FSimpleMulticastDelegate OnWorldEscapeRequested;
    /** I键只发出本地背包开关意图；世界双端控制器不依赖UI实现，不授予或消耗道具。 */
    FSimpleMulticastDelegate OnWorldInventoryRequested;
    FDivineBeastsWorldJournalRequested OnWorldJournalRequested;
    /** 当前本地目标选择/显式重选通知；只读展示意图，技能与交互必须另经服务器资格校验。 */
    FSimpleMulticastDelegate OnWorldTargetChanged;
    /** 最终本地镜头位置、旋转或FOV真实变化后通知；共享世界模块不依赖UI。 */
    FSimpleMulticastDelegate OnFinalCameraViewChanged;
    /** 本地血条显示切换意图；偏好与UI显示归LocalPlayer客户端，不修改战斗数据。 */
    FSimpleMulticastDelegate OnWorldHealthBarsToggleRequested;

    UDivineBeastsPlayerControlsComponent* GetPlayerControls() const { return PlayerControls; }
    AActor* GetSelectedWorldTarget() const { return PlayerControls->GetSelectedWorldTarget(); }
    void SetWorldGameplayInputSuspended(bool bSuspended) { PlayerControls->SetWorldGameplayInputSuspended(bSuspended); }
    void RefreshLocalWorldFacts();
    bool AreLocalResourcesPrepared() const;
    bool IsLocalPawnBound() const;
    bool IsWorldGameplayActive() const;
    virtual void OnRep_PlayerState() override;
    virtual void OnRep_Pawn() override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
    virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
    virtual void FlushPressedKeys() override;
    virtual void UpdateRotation(float DeltaTime) override;
    virtual void UpdateCameraManager(float DeltaSeconds) override;
protected:
    virtual void OnPossess(APawn* NewPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;
    virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FDivineBeastsWorldTargetSelectionTest;
    friend class FDivineBeastsWorldTabTargetCycleTest;
    friend class FDivineBeastsWorldPortalClickTest;
    void BindGameState(AGameStateBase* State);
    TWeakObjectPtr<AGamePlatformPlayerStateBase> ObservedPlayer;
    TWeakObjectPtr<UGamePlatformExperienceComponent> ObservedExperience;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> ObservedCharacter;
    bool bRefreshingFacts=false;
    bool bWorldTravelPending=false;
    /** 非复制默认子对象；仅本地输入处理，专服不启动桌面/Slate行为。 */
    UPROPERTY() TObjectPtr<UDivineBeastsPlayerControlsComponent> PlayerControls;
    void RefreshWorldInputPolicy();
    void ResetWorldInput() { PlayerControls->ResetWorldInput(); }
};
