#pragma once

#include "Screens/DivineBeastsUIScreen.h"
#include "Types/SlateEnums.h"
#include "DivineBeastsCharacterCreateScreen.generated.h"

class UDivineBeastsCharacterCreateViewModel;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;

/** UDivineBeastsCharacterCreateScreen（持久角色创建页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreateScreen
    : public UDivineBeastsUIScreen
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

private:
    /** 根据快照刷新资格、忙碌和错误；保留尚未提交的本地英雄输入。 */
    void RefreshPresentation();
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
