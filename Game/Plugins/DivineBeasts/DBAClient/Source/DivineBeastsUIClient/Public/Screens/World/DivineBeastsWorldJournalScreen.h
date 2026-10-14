// 第三层客户端手册页面：只显示VM状态、提交页签/追踪命令；视觉树必须由Monolith制作。
#pragma once
#include "Screens/World/DivineBeastsWorldScreenBase.h"
#include "Components/ComboBoxString.h"
#include "DivineBeastsWorldJournalScreen.generated.h"

class UButton;
class APawn;
class UTextBlock;
class UDivineBeastsVillageMinimapWidget;
class UDivineBeastsWorldJournalViewModel;

/** 角色信息、地图任务、技能书和公共小队入口的单一本地页面；不暂停网络世界、不发送HTTP。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsWorldJournalScreen : public UDivineBeastsWorldScreenBase
{
    GENERATED_BODY()
protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;
    virtual void RefreshInitialState() override;
    /** Monolith资产命名合同；缺少必需控件应在Widget编译时报错，不运行时生成替代界面。 */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CharacterTab;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> MapQuestTab;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SpellbookTab;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> PartyTab;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> TrackButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> UntrackButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CloseButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> EntryList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> SummaryText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DetailText;
    /** 可选当前区域地图实例；没有有效底图时组件自身保持不可用，不强制通用世界拥有新手村地图。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UDivineBeastsVillageMinimapWidget> WorldMap;
private:
    UDivineBeastsWorldJournalViewModel* ResolveJournal() const;
    /** 活动中换模型时精确解绑旧本地玩家来源，再绑定新模型；外部回调后重验当前选择。 */
    bool ReconcileJournalBinding();
    void RefreshView();
    void RefreshSelection();
    UFUNCTION() void ShowCharacter();
    UFUNCTION() void ShowMapQuest();
    UFUNCTION() void ShowSpellbook();
    UFUNCTION() void ShowParty();
    UFUNCTION() void TrackSelected();
    UFUNCTION() void UntrackSelected();
    UFUNCTION() void CloseJournal();
    UFUNCTION() void HandleEntrySelected(FString Item, ESelectInfo::Type SelectionType);
    /** 选中键属于当前VM快照，不以显示文本/本地化序号当业务身份；数据更新后重新校验。 */
    FName SelectedKey;
    TArray<FName> VisibleKeys;
    bool bRefreshingOptions = false;
    TWeakObjectPtr<APawn> DisplayedMapPawn;
    /** 精确解绑本页面启动的VM，不用后来替换的GetViewModel清理旧来源。 */
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsWorldJournalViewModel> BoundJournal;
};
