#pragma once

#include "CoreMinimal.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
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
    : public UDivineBeastsViewModelBase
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

    /**
     * C++ 热路径只读访问当前 ViewState。
     * 返回 const 引用避免复制 Characters、Arena Scoreboard 等容器；
     * Blueprint 继续使用 GetState() 的值返回接口，保持反射兼容。
     */
    const FDivineBeastsUIViewState& GetStateRef() const { return State; }

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

    /** 仅驱动客户端三维预览，不提交业务选择、不改变ApplicationFlow。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|CharacterPreview")
    bool PreviewCharacterHero(FName HeroDefinitionId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|CharacterPreview")
    void ClearCharacterPreview();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|CharacterPreview")
    void RotateCharacterPreview(float DeltaYawDegrees);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|CharacterPreview")
    void SetCharacterPreviewCameraDistance(float DistanceCentimeters);

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
#if WITH_DEV_AUTOMATION_TESTS
    // 自动化测试只读取命令终态门禁与待处理集合，不向正式蓝图API暴露内部生命周期。
    friend class FDivineBeastsUICommandCompletionLifetimeTest;
#endif

    FGuid Submit(FDivineBeastsUICommand Command);
    void HandleStateChanged(const FDivineBeastsUIViewState& NewState);
    void HandleCommandResult(
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
