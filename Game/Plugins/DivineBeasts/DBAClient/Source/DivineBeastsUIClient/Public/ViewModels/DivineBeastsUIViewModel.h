#pragma once

#include "CoreMinimal.h"
#include "ViewModels/GamePlatformViewModelBase.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "DivineBeastsUIViewModel.generated.h"

class UDivineBeastsUIClientSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FDivineBeastsUICommandCompleted,
    FGuid, RequestId,
    FName, ErrorCode);

/**
 * UDivineBeastsUIViewModel（神兽联盟项目通用ViewModel）。
 * 事件驱动；不强制使用UE5.8 Beta MVVM。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsUIViewModel
    : public UGamePlatformViewModelBase
{
    GENERATED_BODY()

public:
    void InitializeForScreen(
        UDivineBeastsUIClientSubsystem* InOwner,
        FName InScreenId);

    void OnScreenActivated();
    void OnScreenDeactivated();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FName GetScreenId() const { return ScreenId; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FDivineBeastsUIViewState GetState() const { return State; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FName GetLastCommandErrorCode() const { return LastCommandErrorCode; }

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid TryAutoLogin();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid Login(const FString& LoginName, const FString& Password);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid Refresh();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid Retry();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid CreateCharacter(
        FName HeroDefinitionId,
        const FString& CharacterName,
        const TMap<FString, FString>& AppearanceSelection);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid SelectPersistentCharacter(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid RequestWorld(
        FName DesiredExperienceId,
        const FString& PreferredRegion);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid Logout();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid RequestTrainingReset();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid StartMatchmaking(
        FName ArenaModeId,
        const FString& PartyId,
        const FString& PreferredRegion);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid CancelMatchmaking();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid SelectArenaHero(FName HeroDefinitionId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid SetArenaReady();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    FGuid ReturnToWorldAfterMatch();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Command")
    bool CancelCommand(FGuid RequestId);

    UPROPERTY(BlueprintAssignable, Category="DivineBeasts|UI")
    FDivineBeastsUICommandCompleted OnCommandCompleted;

private:
    FGuid Submit(FDivineBeastsUICommand Command);
    void HandleStateChanged(const FDivineBeastsUIViewState& NewState);
    void HandleCommandResult(
        int32 ExpectedVMRevision,
        int32 ExpectedPageGeneration,
        const FDivineBeastsUICommandResult& Result);

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsUIClientSubsystem> Owner = nullptr;

    UPROPERTY(Transient)
    FName ScreenId = NAME_None;

    UPROPERTY(Transient)
    FDivineBeastsUIViewState State;

    UPROPERTY(Transient)
    FName LastCommandErrorCode = NAME_None;

    FDelegateHandle StateHandle;
    TSet<FGuid> PendingCommands;
};
