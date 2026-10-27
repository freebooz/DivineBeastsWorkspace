// SSLM项目层开发客户端页面桥；调用方为DemoWorld/UIManager，只消费页面VM并提交受控目录命令。
// 继承平台Screen，经LocalPlayer层栈打开；Monolith唯一拥有布局/样式/控件树，页面只持显示项。
// 所有接口在游戏线程使用；激活绑定、失活解绑，不能保持旧VM命令源，无HTTP或业务权威。
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
    /** 只允许原生回归检查真实选择委托；不增加运行时测试入口或可视控件树。 */
    friend class FDivineBeastsDemoKeyboardSelectionTest;
    friend class FDivineBeastsIndependentPluginPageTest;
public:
    /** 读取Monolith蓝图默认上下文，WorldUI据此核对软类与插件身份，不改写类默认对象。 */
    FName GetFixedPluginId() const { return FixedPluginId; }
protected:
    /** None为总览；70个Monolith独立页面按其真实PluginId设置，仅默认值可编辑。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Demo") FName FixedPluginId;
    /** 中心显式打开选中插件独立页，依然走目录/Data/UIManager唯一链路。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> OpenPluginPageButton;
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
    /** 显示待审证据索引，不提供自动人工签收；索引是只读输入，使用UHT要求的字符串常量引用，不能作为输出参数修改。 */
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|Demo") void BP_OnEvidenceRequested(const FString& IndexPath);
    /** 中文/稳定身份搜索输入；只改变页面筛选，不重新扫描插件。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UEditableTextBox> PluginSearchInput;
    /** 三层及项目内容包分类；内容包不是第四层。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UComboBoxString> LayerFilterCombo;
    /** 插件只读值列表，选择使用稳定PluginId。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> PluginList;
    /** 当前插件已登记功能；不能用空表断言插件没有能力。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> FeatureList;
    /** 当前功能案例列表；选中与启动分离。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> ScenarioList;
    /** 本次待提交参数副本，输入校验通过后才显式执行。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UListView> ParameterList;
    /** 世界拥有的真实样板控件挂载点；页面解绑时只移除展示，不释放别人的租约。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UNamedSlot> SampleWidgetSlot;
    /** 功能目的、依据、前置与检查步骤，不包含外部可执行文本。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> FeatureDetailsText;
    /** 真实目录/操作/终态结果；受理不能显示为运行成功。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> OperationStatusText;
    /** 实现、接线、审计分母与人工结论分别计数。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> CoverageText;
    /** 显式刷新目录，避免业务Tick扫描。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> RefreshButton;
    /** 通过当前预检后申请进入，失败仍保留可诊断状态。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> LaunchScenarioButton;
    /** 使用本次类型化参数执行当前运行案例。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ExecuteScenarioButton;
    /** 仅案例声明并真实支持Pause时允许。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> PauseScenarioButton;
    /** 仅案例声明并真实支持Resume时允许。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ResumeScenarioButton;
    /** 停止受理新操作并清理本案例拥有的资源。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> StopScenarioButton;
    /** 只恢复案例自己拥有的状态，不回滚已提交业务事务。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ResetDemoButton;
    /** 完成案例清理后申请返回真实开发中心。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> ReturnButton;
    /** 读取已完成运行的待审证据，不代填审核人/通过结论。 */
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
    UFUNCTION() void OnOpenPluginPageClicked();
    UFUNCTION() void OnLaunchClicked();
    UFUNCTION() void OnExecuteClicked();
    UFUNCTION() void OnPauseClicked();
    UFUNCTION() void OnResumeClicked();
    UFUNCTION() void OnStopClicked();
    UFUNCTION() void OnResetClicked();
    UFUNCTION() void OnReturnClicked();
    UFUNCTION() void OnEvidenceClicked();
    /** 仅切换嵌入子页，不另开UI栈、不重启共享VM页面生命周期。 */
    UFUNCTION() void ShowDetailPanel();
    UFUNCTION() void ShowControlsPanel();
    UFUNCTION() void ShowReportPanel();
    /** 鼠标/键盘/手柄共用选择变化；刷新中、旧VM或失活模型不提交选择命令。 */
    void OnPluginSelectionChanged(UObject* Item);
    void OnFeatureSelectionChanged(UObject* Item);
    void OnScenarioSelectionChanged(UObject* Item);
    void ApplySnapshot();
    void SubmitOperation(FName OperationId);
    void DetachViewModel();
};
