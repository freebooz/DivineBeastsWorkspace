// SSLM演示页面桥；继承平台Screen，经平台层栈打开，Monolith拥有布局/样式/控件树。
#pragma once
#include "Screens/GamePlatformUIScreen.h"
#include "UI/DivineBeastsDemoHubViewModel.h"
#include "Components/ComboBoxString.h"
#include "DivineBeastsDemoHubScreen.generated.h"
class UDivineBeastsDemoMenuItem;
class UDivineBeastsDemoParameterItem;

/** 开发中心统一菜单和场景覆盖页，消费ViewModel事件，不在NativeTick扫描业务。 */
UCLASS(Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoHubScreen : public UGamePlatformUIScreen
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    /** 使用平台受激活/页面代次保护的钩子，活动中换VM也会重新绑定当前模型。 */
    virtual void BindUIEvents() override;
    virtual void UnbindUIEvents() override;
    virtual void RefreshInitialState() override;
    virtual void OnViewModelStateChanged(int32 Revision, int32 PageGeneration) override;
    virtual bool NativeOnHandleBackAction() override;
    /** 页面失活解绑视图事件，下一次激活重新读取一次。 */
    virtual void NativeDestruct() override;
    /** Monolith可以按小屏/安全区调整已制作的布局，状态仍来自只读快照。 */
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|Demo") void BP_OnMenuSnapshotChanged(FDivineBeastsDemoMenuSnapshot Snapshot);
    /** 显示待审证据索引，不提供自动人工签收。 */
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|Demo") void BP_OnEvidenceRequested(FString IndexPath);
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UEditableTextBox> PluginSearchInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UComboBoxString> LayerFilterCombo;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> PluginList;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> FeatureList;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> ScenarioList;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> ParameterList;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UNamedSlot> SampleWidgetSlot;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> FeatureDetailsText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> OperationStatusText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> CoverageText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> RefreshButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> LaunchScenarioButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ExecuteScenarioButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> PauseScenarioButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ResumeScenarioButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> StopScenarioButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ResetDemoButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ReturnButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> EvidenceButton;
private:
    /** 只持页面VM和虚拟化项，复制显示数据不拥有插件/世界权威。 */
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsDemoHubViewModel> DemoModel;
    UPROPERTY(Transient) TArray<TObjectPtr<UDivineBeastsDemoMenuItem>> MenuItems;
    UPROPERTY(Transient) TArray<TObjectPtr<UDivineBeastsDemoParameterItem>> ParameterItems;
    /** 仅Editor私有桥创建的待审对象；客户端为空，公开类型只为UObject，不传播Editor依赖。 */
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> PendingEditorReports;
    FName ParametersForScenario;
    bool bSynchronizing = false;
    FDivineBeastsDemoFilter Filter;
    /** 视图更新只用事件，控件变化导致的同步选择事件被抑制。 */
    UFUNCTION() void OnSearchChanged(const FText& Text);
    UFUNCTION() void OnLayerChanged(FString Option, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnLaunchClicked();
    UFUNCTION() void OnExecuteClicked();
    UFUNCTION() void OnPauseClicked();
    UFUNCTION() void OnResumeClicked();
    UFUNCTION() void OnStopClicked();
    UFUNCTION() void OnResetClicked();
    UFUNCTION() void OnReturnClicked();
    UFUNCTION() void OnEvidenceClicked();
    void OnPluginClicked(UObject* Item);
    void OnFeatureClicked(UObject* Item);
    void OnScenarioClicked(UObject* Item);
    void ApplySnapshot();
    void SubmitOperation(FName OperationId);
    void DetachViewModel();
};
