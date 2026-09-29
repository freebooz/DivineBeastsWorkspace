#pragma once

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsCharacterCreateViewModel.generated.h"

/**
 * UDivineBeastsCharacterCreateViewModel（持久角色创建视图模型）。
 * 英雄候选来自 ApplicationFlow（应用流程）公开投影；创建命令仍走统一 UI Command。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreateViewModel
    : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    TArray<FDivineBeastsUICreateHeroItem> GetCreateHeroOptions() const
    {
        return GetStateRef().CreateHeroOptions;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    bool CanSubmitCreation() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return IsPageActive() &&
            !ViewState.bBusy &&
            ViewState.AllowedCommands.Contains(TEXT("CreateCharacter")) &&
            !ViewState.CreateHeroOptions.IsEmpty();
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    bool IsCreationSubmitting() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return ViewState.CurrentStep == TEXT("DBA.Flow.CreateCharacter") ||
            ViewState.PageState == EDivineBeastsUIPageState::Submitting;
    }
};
