#include "Framework/DivineBeastsArenaPlayerController.h"
#include "Gameplay/DivineBeastsPlayerControlsComponent.h"

ADivineBeastsArenaPlayerController::ADivineBeastsArenaPlayerController()
{ PlayerControls = CreateDefaultSubobject<UDivineBeastsPlayerControlsComponent>(TEXT("PlayerControls")); }
void ADivineBeastsArenaPlayerController::SetupInputComponent()
{ Super::SetupInputComponent(); PlayerControls->BindInput(); }
void ADivineBeastsArenaPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{ Super::PostProcessInput(DeltaTime,bGamePaused); PlayerControls->ProcessInput(DeltaTime,bGamePaused); }
void ADivineBeastsArenaPlayerController::UpdateRotation(float DeltaTime)
{ Super::UpdateRotation(DeltaTime); PlayerControls->UpdateObservationRotation(DeltaTime); }
void ADivineBeastsArenaPlayerController::UpdateCameraManager(float DeltaSeconds)
{ Super::UpdateCameraManager(DeltaSeconds); PlayerControls->NotifyFinalCameraView(DeltaSeconds); }
void ADivineBeastsArenaPlayerController::FlushPressedKeys()
{ Super::FlushPressedKeys(); PlayerControls->ResetWorldInput(); }
void ADivineBeastsArenaPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{ Super::ClientRestart_Implementation(NewPawn); PlayerControls->SetTravelPending(false); }
void ADivineBeastsArenaPlayerController::OnRep_PlayerState()
{ Super::OnRep_PlayerState(); PlayerControls->OnInputReceiverChanged.Broadcast(); }
void ADivineBeastsArenaPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{ PlayerControls->SetTravelPending(true); Super::PreClientTravel(PendingURL,TravelType,bIsSeamlessTravel); }
void ADivineBeastsArenaPlayerController::OnUnPossess()
{ PlayerControls->SetGameplayActive(false); Super::OnUnPossess(); }
void ADivineBeastsArenaPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{ PlayerControls->ShutdownControls(); Super::EndPlay(Reason); }
