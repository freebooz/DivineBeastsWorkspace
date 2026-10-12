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
struct FDivineBeastsHeroOverview;

/** UDivineBeastsCharacterCreateScreen（持久角色创建页面基类）。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCharacterCreateScreen
    : public UDivineBeastsCharacterPreviewScreen
{
    GENERATED_BODY()

public:
    /** 跨项目语义化样式默认绑定；具体外观由DBAUIPack_Core资源主题统一决定。 */
    UDivineBeastsCharacterCreateScreen();
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    FName GetCharacterCreateScreenId() const { return TEXT("UI.Screen.CharacterCreate"); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|CharacterCreate")
    UDivineBeastsCharacterCreateViewModel* GetCharacterCreateViewModel() const;

protected:
    /** 激活时绑定本地输入及只读状态事件；失活时全部解绑，不保存业务档案。 */
    virtual void NativeOnActivated() override;
    virtual void RotatePreviewFromDrag(float DeltaYawDegrees) override;
    virtual void NativeOnDeactivated() override;

    /** 介绍事件使用平台安全钩子，支持激活期间更换VM；精确解绑原订阅对象。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;

    /** Monolith页面命名契约；缺少控件必须在蓝图编译时失败。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> CharacterNameInput;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> HeroOptions;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CreateButton;
    /** 空角色也可取消草稿返回选择页；认证、忙碌、加载、错误及流程门禁仍生效，不通过注销模拟取消。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BackToCharacterSelectButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LogoutButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorText;
    /** 现代布局可选适配；旧已发布页面保留下拉合同，缺少磁贴资产时仍可操作。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWrapBox> HeroChoices;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SelectedHeroName;
    /** 旧蓝图兼容可选绑定；新Monolith资产门禁必须确认三控件实际存在、位于独立滚动区。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HeroOverviewStatusText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HeroIntroductionText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillIntroductionText;
    /** Monolith生成的项目磁贴类；只依赖项目UI内容，不引入竞技或后端权威。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|UI|Characters") TSubclassOf<UDivineBeastsCharacterChoiceEntry> ChoiceEntryClass;

private:
    /** 仅建立介绍订阅/投影，不加载资源、不格式化权威技能数值，不改按钮或旋转区域。 */
    bool ReconcileHeroOverviewBinding();
    void HandleHeroOverviewChanged(const FDivineBeastsHeroOverview& Overview);
    void ApplyHeroOverviewText(const FDivineBeastsHeroOverview& Overview);
    void ReleaseHeroOverviewBinding();
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsCharacterCreateViewModel> BoundHeroOverviewViewModel = nullptr;
    FDelegateHandle HeroOverviewHandle;
    int32 BoundHeroOverviewPageGeneration = 0;
    bool bHeroOverviewEventsBound = false;
    /** 根据快照刷新资格、忙碌和错误；保留尚未提交的本地英雄输入。 */
    void RefreshPresentation();
    /** 单轮投影只属于捕获的VM/页面代次；同步观察者换页后立即停止旧栈。 */
    void RefreshPresentationPass(UDivineBeastsCharacterCreateViewModel* VM, int32 ExpectedPageGeneration);
    /** 有界同步补刷后的额外重入合并为单条弱页面任务；失活代次取消，不使用业务Tick。 */
    void QueuePendingPresentationRefresh();
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
    /** 仅返回角色选择界面，未提交创建草稿随页面失活释放。 */
    UFUNCTION() void HandleBackToCharacterSelect();
    UFUNCTION() void HandleLogout();
    /** 下拉项与快照ID的显示索引，仅用于组装用户命令，不是业务选择状态。 */
    TArray<FName> PresentedHeroIds;
    /** 下拉框重建期间忽略内部选择事件；外部刷新请求另存pending，不递归执行。 */
    bool bRefreshing = false;
    bool bPresentationRefreshPending = false;
    bool bPresentationRefreshQueued = false;
    /** 页面绑定寿命栅栏，防旧弱任务刷新失活后重新激活的同一Widget。 */
    uint64 PresentationRefreshGeneration = 0;
};
