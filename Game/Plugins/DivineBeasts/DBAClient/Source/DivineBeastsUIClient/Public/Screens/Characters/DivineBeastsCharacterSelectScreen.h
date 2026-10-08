#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "Types/SlateEnums.h"
#include "DivineBeastsCharacterSelectScreen.generated.h"

class UDivineBeastsCharacterSelectViewModel;
class UButton;
class UComboBoxString;
class UTextBlock;

/** UDivineBeastsCharacterSelectScreen（持久角色选择页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterSelectScreen
    : public UDivineBeastsUIScreen
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    FName GetCharacterSelectScreenId() const { return TEXT("UI.Screen.CharacterSelect"); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    UDivineBeastsCharacterSelectViewModel* GetCharacterSelectViewModel() const;

protected:
    /** 只在页面激活期间订阅输入、状态和命令完成事件，无业务Tick。 */
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CharacterList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SelectButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RotateLeftButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RotateRightButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogoutButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;

private:
    /** 投影真实角色列表并刷新可提交条件，后端拥有角色资格和正式选择。 */
    void RefreshPresentation();
    /** 平台事件参数为修订号与页面代次；只刷新档案投影，不持有第二份权威选择。 */
    UFUNCTION() void HandleStateChanged(int32 Revision, int32 PageGeneration);
    UFUNCTION() void HandleCommandCompleted(FGuid RequestId, FName ErrorCode);
    UFUNCTION() void HandleCharacterChanged(FString Item, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleSelect();
    UFUNCTION() void HandleRotateLeft();
    UFUNCTION() void HandleRotateRight();
    UFUNCTION() void HandleLogout();
    /** 仅缓存显示项对应ID；提交和预览前重新查询当前只读快照，拒绝失效项。 */
    TArray<FString> PresentedCharacterIds;
    bool bRefreshing = false;
};
