#pragma once

#include "Screens/Characters/DivineBeastsCharacterPreviewScreen.h"
#include "DivineBeastsCharacterSelectScreen.generated.h"

class UDivineBeastsCharacterSelectViewModel;
class UButton;
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
    /** 跨项目语义化样式默认绑定；具体外观由DBAUIPack_Core资源主题统一决定。 */
    UDivineBeastsCharacterSelectScreen();
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    FName GetCharacterSelectScreenId() const { return TEXT("UI.Screen.CharacterSelect"); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterSelect")
    UDivineBeastsCharacterSelectViewModel* GetCharacterSelectViewModel() const;

protected:
    /** 只在页面激活期间订阅输入、状态和命令完成事件，无业务Tick。 */
    virtual void NativeOnActivated() override;
    virtual void RotatePreviewFromDrag(float DeltaYawDegrees) override;
    virtual void AdjustPreviewCameraDistanceFromWheel(float DeltaCentimeters) override;
    virtual void AdjustPreviewCameraFocalLengthFromWheel(float DeltaMillimeters) override;
    virtual void NativeOnDeactivated() override;
    /** 空档案优先聚焦可见、启用且允许键盘焦点的创建入口；非空档案沿用当前卡片及原有回退。 */
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SelectButton;
    /** 主按钮空档案显示创建、有档案显示进入游戏；旧内容过渡兼容，正式资产门禁要求真实绑定。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SelectButtonLabel;
    /** 创建入口由Monolith公共页面提供；旧资产可暂缺，正式资产门禁检查它必须存在。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> OpenCharacterCreateButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogoutButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
    /** 真实滚动列表的必需宿主；不再维护隐藏下拉框或旧选项索引。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> CharacterChoices;
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
    UFUNCTION() void HandleSelect();
    /** 只打开创建表单，提交创建仍由创建页经统一命令端口执行。 */
    UFUNCTION() void HandleOpenCharacterCreate();
    UFUNCTION() void HandleLogout();
    /** 仅缓存显示项对应ID；提交和预览前重新查询当前只读快照，拒绝失效项。 */
    TArray<FString> PresentedCharacterIds;
    /** 页面内当前浏览档案的稳定ID，失活清空；只决定高亮和预览，不改写后端SelectedCharacterId。 */
    UPROPERTY(Transient) FString PreviewedCharacterId;
    bool bRefreshing = false;
};
