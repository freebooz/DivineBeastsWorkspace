// 第三层双端世界控制器：桥接平台准备事实；本地PC输入策略拥有当前Pawn的鼠标拖动/观察镜头。
// 不依赖客户端UI，菜单由中立委托交给项目客户端；不提交坐标RPC或放宽服务器资格。
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "Framework/GamePlatformGameStateBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Gameplay/DivineBeastsWorldControlState.h"
#include "Components/InputComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#if !UE_SERVER
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#endif

DECLARE_DELEGATE_OneParam(FDivineBeastsJournalInputDelegate, FName);

ADivineBeastsWorldPlayerController::ADivineBeastsWorldPlayerController()
{
    auto* Interactor = CreateDefaultSubobject<UGamePlatformInteractorComponent>(TEXT("WorldInteractor"));
    Interactor->SetLocalFocusPolicy(false, true);
    PlayerControls = CreateDefaultSubobject<UDivineBeastsPlayerControlsComponent>(TEXT("PlayerControls"));
    PlayerControls->OnWorldEscapeRequested.AddWeakLambda(this, [this] { OnWorldEscapeRequested.Broadcast(); });
    PlayerControls->OnWorldInventoryRequested.AddWeakLambda(this, [this] { OnWorldInventoryRequested.Broadcast(); });
    PlayerControls->OnWorldTargetChanged.AddWeakLambda(this, [this] { OnWorldTargetChanged.Broadcast(); });
    PlayerControls->OnFinalCameraViewChanged.AddWeakLambda(this, [this] { OnFinalCameraViewChanged.Broadcast(); });
    PlayerControls->OnWorldHealthBarsToggleRequested.AddWeakLambda(this, [this] { OnWorldHealthBarsToggleRequested.Broadcast(); });
    PlayerControls->OnWorldJournalRequested.AddWeakLambda(this, [this](FName Section) { OnWorldJournalRequested.Broadcast(Section); });
}

void ADivineBeastsWorldPlayerController::SetupInputComponent() { Super::SetupInputComponent(); PlayerControls->BindInput(); }
void ADivineBeastsWorldPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused) { Super::PostProcessInput(DeltaTime,bGamePaused); PlayerControls->ProcessInput(DeltaTime,bGamePaused); }
void ADivineBeastsWorldPlayerController::UpdateRotation(float DeltaTime) { Super::UpdateRotation(DeltaTime); PlayerControls->UpdateObservationRotation(DeltaTime); }
void ADivineBeastsWorldPlayerController::UpdateCameraManager(float DeltaSeconds) { Super::UpdateCameraManager(DeltaSeconds); PlayerControls->NotifyFinalCameraView(DeltaSeconds); }
void ADivineBeastsWorldPlayerController::FlushPressedKeys() { Super::FlushPressedKeys(); PlayerControls->ResetWorldInput(); }
void ADivineBeastsWorldPlayerController::RefreshWorldInputPolicy() { PlayerControls->SetGameplayActive(IsWorldGameplayActive() && !bWorldTravelPending); }
int32 ADivineBeastsWorldPlayerController::GetGameplayAvatarGeneration() const
{
    const int32 Generation = Super::GetGameplayAvatarGeneration();
    if (Generation <= 0) return 0;
    const APawn* ControlledPawn = GetPawn();
    const auto* ASC = ControlledPawn ? ControlledPawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>() : nullptr;
    // 项目Gate已经核对角色Ready、身份/战斗代次、死亡和服务器Active；
    // 复用同一查询，防止死亡后的五秒重生等待仍因平台生命周期Active而打开竞技门。
    return ASC && ASC->EvaluateActivationEligibility().IsSuccess() ? Generation : 0;
}

void ADivineBeastsWorldPlayerController::ServerCaptureVillageQueueOrigin_Implementation(const FString& QueueTicketId)
{
    if (!HasAuthority() || QueueTicketId.IsEmpty() || QueueTicketId.Len() > 128)
    {
        return;
    }
    OnVillageQueueOriginRequested.Broadcast(QueueTicketId);
}

void ADivineBeastsWorldPlayerController::BeginPlay()
{
    Super::BeginPlay();
    OnPreparationChanged.AddUObject(this,&ThisClass::RefreshLocalWorldFacts);
    GetWorld()->GameStateSetEvent.AddUObject(this,&ThisClass::BindGameState);
    BindGameState(GetWorld()->GetGameState()); RefreshLocalWorldFacts();
}

void ADivineBeastsWorldPlayerController::BindGameState(AGameStateBase* State)
{
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.RemoveAll(this);
    auto* Shared=Cast<AGamePlatformGameStateBase>(State);
    ObservedExperience=Shared?Shared->GetExperienceComponent():nullptr;
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.AddUObject(this,&ThisClass::RefreshLocalWorldFacts);
    RefreshLocalWorldFacts();
}

bool ADivineBeastsWorldPlayerController::AreLocalResourcesPrepared() const
{
    return ObservedExperience.IsValid() && ObservedExperience->GetClientExperienceSnapshot().Stage==EGamePlatformClientExperienceStage::Prepared;
}

bool ADivineBeastsWorldPlayerController::IsLocalPawnBound() const
{
    return ObservedPlayer.IsValid() && GetPawn() && GetPawn()->GetController()==this
        && ObservedPlayer->GetLifecycleSnapshot().ControlledPawn==GetPawn() && GetPawn()->InputComponent;
}

bool ADivineBeastsWorldPlayerController::IsWorldGameplayActive() const
{
    return ObservedPlayer.IsValid() && ObservedPlayer->GetLifecycleSnapshot().IsServerActive() && IsLocalPawnBound()
        && ObservedCharacter.IsValid() && ObservedCharacter->IsCharacterReady();
}

void ADivineBeastsWorldPlayerController::RefreshLocalWorldFacts()
{
    if(!IsLocalController() || bRefreshingFacts) return;
    TGuardValue<bool> Guard(bRefreshingFacts,true);
    auto* State=GetPlayerState<AGamePlatformPlayerStateBase>();
    if(ObservedPlayer.Get()!=State)
    {
        if(ObservedPlayer.IsValid()) ObservedPlayer->OnLifecycleChanged.RemoveAll(this);
        ObservedPlayer=State;
        if(State) State->OnLifecycleChanged.AddUObject(this,&ThisClass::RefreshLocalWorldFacts);
    }
    auto* CurrentCharacterIdentity = GetPawn() ? GetPawn()->FindComponentByClass<UDivineBeastsCharacterComponent>() : nullptr;
    if (ObservedCharacter.Get() != CurrentCharacterIdentity)
    {
        if (ObservedCharacter.IsValid()) ObservedCharacter->OnReadinessChanged().RemoveAll(this);
        ObservedCharacter = CurrentCharacterIdentity;
        if (CurrentCharacterIdentity) CurrentCharacterIdentity->OnReadinessChanged().AddWeakLambda(this, [this](bool) { RefreshLocalWorldFacts(); });
    }
    // Prepare令牌只提交一次，必须等项目必要定义Ready后再报告，避免被服务器拒绝后没有新事件重试。
    if(AreLocalResourcesPrepared() && IsLocalPawnBound() && CurrentCharacterIdentity && CurrentCharacterIdentity->IsCharacterReady())
    {
        FGamePlatformLocalPreparationFacts Facts;
        Facts.bClientExperiencePrepared=AreLocalResourcesPrepared(); Facts.bClientPawnBound=IsLocalPawnBound();
        Facts.bInputProfilePrepared=GetPawn()->InputComponent!=nullptr; Facts.bBindingsReady=Facts.bInputProfilePrepared;
        ReportLocalPreparation(Facts);
    }
    RefreshWorldInputPolicy();
    OnLocalWorldFactsChanged.Broadcast();
}

void ADivineBeastsWorldPlayerController::OnRep_PlayerState(){Super::OnRep_PlayerState();RefreshLocalWorldFacts();}

void ADivineBeastsWorldPlayerController::OnRep_Pawn(){Super::OnRep_Pawn();RefreshLocalWorldFacts();}

void ADivineBeastsWorldPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
    Super::ClientRestart_Implementation(NewPawn);
    // 只有引擎接收服务器重新拥有当前Pawn的回执才撤销旅行门闩；旧鼠标状态不跨世界保留。
    bWorldTravelPending = false;
    PlayerControls->SetTravelPending(false);
    RefreshLocalWorldFacts();
}

void ADivineBeastsWorldPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
    bWorldTravelPending = true;
    PlayerControls->SetTravelPending(true);
    ResetWorldInput();
    RefreshWorldInputPolicy();
    Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ADivineBeastsWorldPlayerController::OnPossess(APawn* NewPawn)
{
    Super::OnPossess(NewPawn);
    RefreshLocalWorldFacts();
}

void ADivineBeastsWorldPlayerController::OnUnPossess()
{
    ResetWorldInput();
    Super::OnUnPossess();
    RefreshLocalWorldFacts();
}
void ADivineBeastsWorldPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    PlayerControls->ShutdownControls();
    OnPreparationChanged.RemoveAll(this); GetWorld()->GameStateSetEvent.RemoveAll(this);
    if(ObservedPlayer.IsValid()) ObservedPlayer->OnLifecycleChanged.RemoveAll(this);
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.RemoveAll(this);
    if(ObservedCharacter.IsValid()) ObservedCharacter->OnReadinessChanged().RemoveAll(this);
    OnLocalWorldFactsChanged.Clear(); OnWorldEscapeRequested.Clear(); OnWorldInventoryRequested.Clear();
    OnWorldJournalRequested.Clear(); OnWorldTargetChanged.Clear(); OnFinalCameraViewChanged.Clear(); OnWorldHealthBarsToggleRequested.Clear();
    Super::EndPlay(Reason);
}
