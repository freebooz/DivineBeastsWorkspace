// 演示UI蓝图的项目层纯桥接基类；视觉布局和交互事件须由Monolith MCP制作。
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Components/ComboBoxString.h"

#include "DivineBeastsDemoHubWidget.generated.h"

/** WBP_DBA_PluginDemoHub等真实UI蓝图的父类；不在C++构建另一套可视控件树。 */
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

private:
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
