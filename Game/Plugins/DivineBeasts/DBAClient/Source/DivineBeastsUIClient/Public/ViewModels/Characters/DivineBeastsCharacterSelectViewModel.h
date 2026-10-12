#pragma once

#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
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
    /** 返回事件快照中的档案副本；显示数据不授予角色选择或世界准入权。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    TArray<FDivineBeastsUICharacterItem> GetCharacters() const
    {
        return GetStateRef().Characters;
    }

    /** 判断当前快照是否有档案；加载是否完成仍由CurrentStep/Loading决定。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool HasCharacters() const
    {
        return !GetStateRef().Characters.IsEmpty();
    }

    /** 已激活、已认证、空闲无错误的角色入口且有档案/选择命令才允许提交；后端仍验证具体ID资格。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool CanSubmitSelection() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return IsPageActive() &&
            FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(ViewState, TEXT("UI.Screen.CharacterSelect")) &&
            ViewState.AllowedCommands.Contains(TEXT("SelectPersistentCharacter")) &&
            !ViewState.Characters.IsEmpty();
    }

    /** 空档案主按钮或辅助按钮可打开创建表单的只读资格；不等同创建成功或进入世界授权。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool CanOpenCharacterCreation() const
    {
        return IsPageActive() && FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(
            GetStateRef(), TEXT("UI.Screen.CharacterCreate"));
    }

    /** 只读显示正式验证节点或提交页状态，不轮询请求、不延长命令生命周期。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    bool IsSelectionSubmitting() const
    {
        const FDivineBeastsUIViewState& ViewState = GetStateRef();
        return ViewState.CurrentStep == TEXT("DBA.Flow.ValidateSelection") ||
            ViewState.PageState == EDivineBeastsUIPageState::Submitting;
    }
};
