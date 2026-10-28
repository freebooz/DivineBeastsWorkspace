// 演示UI蓝图的项目层纯桥接基类；视觉布局和交互事件须由Monolith MCP制作。
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Components/ComboBoxString.h"

#include "DivineBeastsDemoHubWidget.generated.h"
class UDivineBeastsDemoHubViewModel;
struct FDivineBeastsDemoScenarioDefinition;
class UDivineBeastsDemoParameterItem;

/** 既有兼容开发界面与Detail/Controls/Report子页的父类；主Hub已派生HubScreen。
 * 子页借用主Hub模型，不能拥有其页面代次；此类型不在C++构建另一套可视控件树。 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoHubWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** 与Monolith真实命名控件绑定；仅处理目录/指令，不在C++创建任何视觉控件。 */
    virtual void NativeOnInitialized() override;

    /** 当前只读插件目录；可展示已禁用插件但不能假装能启动。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoPlugin> GetDiscoveredPlugins() const;

    /** 中文/英文ID模糊搜索与架构分层筛选；只过滤已发现目录，不猜测未安装插件。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoPlugin> FilterDiscoveredPlugins(const FString& SearchText,
        bool bFilterLayer, EDivineBeastsDemoLayer Layer, bool bOnlyWithMaps) const;

    /** 按已选插件返回真实关卡候选。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    TArray<FDivineBeastsDemoScenario> GetPluginScenarios(FName PluginId) const;

    /** 提交受控地图跳转请求；false显示目录服务的错误状态。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool RequestOpenScenario(FName ScenarioId);

    /** 从演示场景返回总览；当前仅负责请求，不承诺地图完成加载。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool RequestReturnToHub();

    /** 开发演示的最新请求诊断。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FText GetDemoOperationMessage() const;

    /** 详情/控制/报告子页借用Hub拥有的唯一模型；不BeginPage、不绑定目录或拥有执行器，空值精确解绑。 */
    void BindPresentationModel(UDivineBeastsDemoHubViewModel* InModel);

protected:
    /** 子页在真实构造期间接按钮，销毁时撤销模型事件与样板挂载；兼容旧演示覆盖页。 */
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    /** 页面模型为弱借用；参数是本页的开发输入副本，待审对象只在Editor桥创建且不自动签收。 */
    TWeakObjectPtr<UDivineBeastsDemoHubViewModel> PresentationModel;
    int32 PresentationPageGeneration = 0;
    UPROPERTY(Transient) TArray<TObjectPtr<UDivineBeastsDemoParameterItem>> PanelParameterItems;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> PanelPendingReports;
    FName PanelParameterScenario;
    /** 同案例目录更新后也需重建参数合同，不能只用ID沿用旧范围或默认值。 */
    FString PanelParameterFingerprint;
    FString PanelEvidencePath;
    FGuid PanelEvidenceRunId;
    FText PanelCommandFeedback;
    /** 控制页防止同步广播时的按钮重入；参数列表只在合同切换时重建。 */
    bool bPanelCommandDispatching = false;
    void RefreshPanelPresentation();
    bool HasCurrentPresentationModel() const;
    void SubmitPanelOperation(FName OperationId);
    /** 从只读案例合同构建具名选择；控件树仍由Monolith制作。 */
    void RefreshPanelOperationChoices(const FDivineBeastsDemoScenarioDefinition* Scenario, bool bCanExecute);
    TArray<FName> PanelOperationIds;
    FName PanelOperationsScenario;
    FString PanelOperationsFingerprint;
    bool bRefreshingPanelOperationChoices = false;
    bool bPendingPanelOperationChoiceRefresh = false;
    UFUNCTION() void PanelRunSelectedOperation();
    UFUNCTION() void PanelModelChanged(int32 Revision, int32 PageGeneration);
    UFUNCTION() void PanelLaunch();
    UFUNCTION() void PanelExecute();
    UFUNCTION() void PanelPause();
    UFUNCTION() void PanelResume();
    UFUNCTION() void PanelStop();
    UFUNCTION() void PanelReset();
    UFUNCTION() void PanelEvidence();
    /** Monolith WBP中的可选命名控件，BirdFlock/Overlay派生界面未包含时保持空值。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UEditableTextBox> PluginSearchInput;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UComboBoxString> LayerFilterCombo;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UComboBoxString> PluginSelectorCombo;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UComboBoxString> ScenarioSelectorCombo;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> RefreshButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ClearFilterButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> LaunchScenarioButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ReturnButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ResetDemoButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> PluginCountText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> PluginDescriptionText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> OperationStatusText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> OverlayFeedbackText;

    /** UI刷新只遍历已缓存目录；拦截刷新期间ComboBox的重入事件。 */
    bool bSynchronizingDemoUi = false;
    TMap<FString, FName> ScenarioByOption;
    void RefreshPluginControls();
    void RefreshScenarioControls();
    void UpdateScenarioStatus();
    void UpdateOverlayStatus();
    UFUNCTION() void OnSearchChanged(const FText& NewText);
    UFUNCTION() void OnLayerChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnPluginChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnScenarioChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnCatalogRefreshClicked();
    UFUNCTION() void OnFilterResetClicked();
    UFUNCTION() void OnScenarioLaunchClicked();
    UFUNCTION() void OnHubReturnClicked();
    UFUNCTION() void OnDemoResetClicked();
    UDivineBeastsDemoCatalogSubsystem* GetCatalog() const;
};
