#pragma once

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "DivineBeastsCharacterSelectViewModel.generated.h"

/**
 * UDivineBeastsCharacterSelectViewModel（持久角色选择视图模型）。
 * 只消费统一 ViewState（视图状态）并复用 SelectPersistentCharacter 命令，
 * 不保存第二份业务选择状态、不直接访问后端。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterSelectViewModel
    : public UDivineBeastsUIViewModel
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    TArray<FDivineBeastsUICharacterItem> GetCharacters() const
    {
        return GetStateRef().Characters;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool HasCharacters() const
    {
        return !GetStateRef().Characters.IsEmpty();
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool CanSubmitSelection() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return IsPageActive() &&
            !ViewState.bBusy &&
            ViewState.AllowedCommands.Contains(TEXT("SelectPersistentCharacter")) &&
            !ViewState.Characters.IsEmpty();
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool IsSelectionSubmitting() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return ViewState.CurrentStep == TEXT("DBA.Flow.ValidateSelection") ||
            ViewState.PageState == EDivineBeastsUIPageState::Submitting;
    }
};
