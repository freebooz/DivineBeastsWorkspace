#pragma once

#include "Screens/Characters/DivineBeastsCharacterPreviewScreen.h"
#include "Types/SlateEnums.h"
#include "DivineBeastsCharacterSelectScreen.generated.h"

class UDivineBeastsCharacterSelectViewModel;
class UButton;
class UComboBoxString;
class UTextBlock;
class UVerticalBox;
class UDivineBeastsCharacterChoiceEntry;

/** UDivineBeastsCharacterSelectScreen（持久角色选择页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterSelectScreen
    : public UDivineBeastsCharacterPreviewScreen
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
    virtual void RotatePreviewFromDrag(float DeltaYawDegrees) override;
    virtual void NativeOnDeactivated() override;
    /** 可见卡片按钮优先获得焦点，旧页面回退到原有下拉控件。 */
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> CharacterList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SelectButton;
    /** 创建入口由Monolith公共页面提供；旧资产可暂缺，正式资产门禁检查它必须存在。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> OpenCharacterCreateButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogoutButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
    /** 只读角色列表的现代布局宿主；缺少时保留旧页面合同。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> CharacterChoices;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedHeroName;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedCharacterName;
    /** Monolith角色卡片资产类型，由项目内容包持有样式与布局。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|UI|Characters") TSubclassOf<UDivineBeastsCharacterChoiceEntry> ChoiceEntryClass;

private:
    /** 投影真实角色列表并刷新可提交条件，后端拥有角色资格和正式选择。 */
    void RefreshPresentation();
    /** 列表只按稳定档案身份重建，文案、资格和选中外框按状态事件刷新。 */
    void RebuildCharacterChoices();
    void RefreshCharacterChoices();
    void ClearCharacterChoices();
    void HandleChoiceRequested(UDivineBeastsCharacterChoiceEntry* Entry);
    UPROPERTY(Transient) TArray<TObjectPtr<UDivineBeastsCharacterChoiceEntry>> ChoiceEntries;
    /** 平台事件参数为修订号与页面代次；只刷新档案投影，不持有第二份权威选择。 */
    UFUNCTION() void HandleStateChanged(int32 Revision, int32 PageGeneration);
    UFUNCTION() void HandleCommandCompleted(FGuid RequestId, FName ErrorCode);
    UFUNCTION() void HandleCharacterChanged(FString Item, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleSelect();
    /** 只打开创建表单，提交创建仍由创建页经统一命令端口执行。 */
    UFUNCTION() void HandleOpenCharacterCreate();
    UFUNCTION() void HandleLogout();
    /** 仅缓存显示项对应ID；提交和预览前重新查询当前只读快照，拒绝失效项。 */
    TArray<FString> PresentedCharacterIds;
    bool bRefreshing = false;
};
