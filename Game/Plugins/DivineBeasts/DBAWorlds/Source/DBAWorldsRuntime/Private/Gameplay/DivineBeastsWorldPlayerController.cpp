// 项目世界控制器仅桥接平台事实与本地输入；网络权限仍由真实服务器生命周期验证。
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Framework/GamePlatformGameStateBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/DivineBeastsCharacterComponent.h"
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
    SetIgnoreMoveInput(!IsWorldGameplayActive()); SetIgnoreLookInput(!IsWorldGameplayActive());
    if(IsWorldGameplayActive()) { ResetIgnoreMoveInput(); ResetIgnoreLookInput(); bShowMouseCursor=false; SetInputMode(FInputModeGameOnly()); }
    OnLocalWorldFactsChanged.Broadcast();
}
void ADivineBeastsWorldPlayerController::OnRep_PlayerState(){Super::OnRep_PlayerState();RefreshLocalWorldFacts();}
void ADivineBeastsWorldPlayerController::OnRep_Pawn(){Super::OnRep_Pawn();RefreshLocalWorldFacts();}
void ADivineBeastsWorldPlayerController::ClientRestart_Implementation(APawn* NewPawn){Super::ClientRestart_Implementation(NewPawn);RefreshLocalWorldFacts();}
void ADivineBeastsWorldPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    OnPreparationChanged.RemoveAll(this); GetWorld()->GameStateSetEvent.RemoveAll(this);
    if(ObservedPlayer.IsValid()) ObservedPlayer->OnLifecycleChanged.RemoveAll(this);
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.RemoveAll(this);
    if(ObservedCharacter.IsValid()) ObservedCharacter->OnReadinessChanged().RemoveAll(this);
    OnLocalWorldFactsChanged.Clear(); Super::EndPlay(Reason);
}
