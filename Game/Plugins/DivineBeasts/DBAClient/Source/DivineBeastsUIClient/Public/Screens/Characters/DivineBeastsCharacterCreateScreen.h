#pragma once

#include "Screens/Characters/DivineBeastsCharacterPreviewScreen.h"
#include "Types/SlateEnums.h"
#include "DivineBeastsCharacterCreateScreen.generated.h"

class UDivineBeastsCharacterCreateViewModel;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UWrapBox;
class UDivineBeastsCharacterChoiceEntry;

/** UDivineBeastsCharacterCreateScreen（持久角色创建页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreateScreen
    : public UDivineBeastsCharacterPreviewScreen
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    FName GetCharacterCreateScreenId() const { return TEXT("UI.Screen.CharacterCreate"); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    UDivineBeastsCharacterCreateViewModel* GetCharacterCreateViewModel() const;

protected:
    /** 激活时绑定本地输入及只读状态事件；失活时全部解绑，不保存业务档案。 */
    virtual void NativeOnActivated() override;
    virtual void RotatePreviewFromDrag(float DeltaYawDegrees) override;
    virtual void NativeOnDeactivated() override;

    /** Monolith页面命名契约；缺少控件必须在蓝图编译时失败。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> CharacterNameInput;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> HeroOptions;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CreateButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RotateLeftButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RotateRightButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogoutButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
    /** 现代布局可选适配；旧已发布页面保留下拉合同，缺少磁贴资产时仍可操作。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> HeroChoices;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedHeroName;
    /** Monolith生成的项目磁贴类；只依赖项目UI内容，不引入竞技或后端权威。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|UI|Characters") TSubclassOf<UDivineBeastsCharacterChoiceEntry> ChoiceEntryClass;

private:
    /** 根据快照刷新资格、忙碌和错误；保留尚未提交的本地英雄输入。 */
    void RefreshPresentation();
    /** 重建／释放本地磁贴投影，按身份同步当前选择，避免资格变化时误选旧索引。 */
    void RebuildHeroChoices();
    void RefreshHeroChoices();
    void ClearHeroChoices();
    void HandleChoiceRequested(UDivineBeastsCharacterChoiceEntry* Entry);
    UPROPERTY(Transient) TArray<TObjectPtr<UDivineBeastsCharacterChoiceEntry>> ChoiceEntries;
    /** 平台事件参数分别为状态修订号和页面代次；事件只触发只读显示刷新。 */
    UFUNCTION() void HandleStateChanged(int32 Revision, int32 PageGeneration);
    /** 创建完成（含失败）刷新反馈；后端错误不在页面内吞掉或伪造成功。 */
    UFUNCTION() void HandleCommandCompleted(FGuid RequestId, FName ErrorCode);
    /** 本地名称/英雄输入变化只重算可用性与预览，提交前仍重新核对快照资格。 */
    UFUNCTION() void HandleNameChanged(const FText& Text);
    UFUNCTION() void HandleHeroChanged(FString Item, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleCreate();
    /** 左右转动每次30度，仅影响本地舞台；注销通过统一命令端口处理。 */
    UFUNCTION() void HandleRotateLeft();
    UFUNCTION() void HandleRotateRight();
    UFUNCTION() void HandleLogout();
    /** 下拉项与快照ID的显示索引，仅用于组装用户命令，不是业务选择状态。 */
    TArray<FName> PresentedHeroIds;
    /** 下拉框重建期间忽略内部选择事件，防止递归刷新和重复预览。 */
    bool bRefreshing = false;
};
