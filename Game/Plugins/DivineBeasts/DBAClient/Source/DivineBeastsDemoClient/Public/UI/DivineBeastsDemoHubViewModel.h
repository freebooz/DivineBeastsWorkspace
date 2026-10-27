// SSLM项目演示ViewModel；本地页面拥有投影，目录/世界拥有执行事实，全部游戏线程事件驱动。
#pragma once
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "Contracts/DivineBeastsDemoMenuTypes.h"
#include "DivineBeastsDemoHubViewModel.generated.h"
class UDivineBeastsDemoWorldSubsystem;
class UUserWidget;

/** 目录和世界只读投影，提交受控命令；无Tick/HTTP/业务权威。 */
UCLASS(BlueprintType)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoHubViewModel : public UDivineBeastsViewModelBase
{
    GENERATED_BODY()
    /** 回归仅注入只读菜单值夹具，不连接后端、不改变目录或世界权威。 */
    friend class FDivineBeastsDemoKeyboardSelectionTest;
    friend class FDivineBeastsIndependentPluginPageTest;
public:
    /** 固定PluginId由页面默认值/受控路由提供；None为中心，活动已绑定页不允许改写。 */
    bool ConfigurePluginContext(FName PluginId);
    /** 只读页面固定身份；搜索与选择不能更改。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo") FName GetFixedPluginId() const { return FixedPluginId; }
    /** 中心打开选中插件独立页；固定页只允许请求自己的身份，目录再次核验。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool RequestPluginPage(FName PluginId);
    /** 绑定实际GI目录；活动页面立即订阅/抓取一次，空值返回false并清理旧绑定。 */
    bool BindCatalog(UDivineBeastsDemoCatalogSubsystem* InCatalog);
    /** 失活/销毁解绑委托和世界弱引用，不停止其他页面或世界的运行。 */
    void UnbindCatalog();
    /** 设置筛选值只更新投影，不扫描资源或改变总量。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") void SetFilter(const FDivineBeastsDemoFilter& InFilter);
    /** 按稳定ID选择当前可见插件；不按显示名称猜测。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool SelectPlugin(FName PluginId);
    /** 按稳定ID选择当前插件功能；不存在/不可见返回false。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool SelectFeature(FName FeatureId);
    /** 选择当前功能的可见案例，仅改投影，不执行。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool SelectScenario(FName ScenarioId);
    /** 先真实预检再提交目录进入请求，返回受理而非运行成功。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool RequestScenario(FName ScenarioId);
    /** 将显式类型化参数交给本世界运行，能力缺失/忙碌返回false。 */
    bool RequestOperation(FName OperationId, const TArray<FDivineBeastsDemoParameterValue>& Parameters);
    /** 人工刷新目录；失活页面拒绝发起新请求。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") void RequestRefresh();
    /** 先停止当前案例再返回Hub，清理失败保持原世界诊断。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo") bool RequestReturn();
    /** 自包含只读快照，订阅平台OnViewStateChanged后读取。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo") FDivineBeastsDemoMenuSnapshot GetMenuSnapshot() const { return Snapshot; }
    /** 只读取绑定世界的已创建样板，供NamedSlot显示。 */
    UUserWidget* GetSampleWidget() const;
    /** 将当前运行写成待审证据，返回真实索引路径；失活或未运行返回空并提供错误。 */
    FString RequestEvidence(FString& OutError);
protected:
    virtual void OnPageBegan() override;
    virtual void OnPageEnded() override;
    virtual void BeginDestroy() override;
private:
    /** 页面外不保活业务实例，换页/旅行后旧事件必须解绑。 */
    TWeakObjectPtr<UDivineBeastsDemoCatalogSubsystem> Catalog;
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> BoundWorld;
    FDelegateHandle CatalogHandle;
    FDelegateHandle RunHandle;
    FDivineBeastsDemoFilter Filter;
    FName FixedPluginId;
    /** 重绑与同页重开均递增，已投递委托必须同时核对源和代次。 */
    uint64 BindingGeneration = 0;
    bool IsPluginEnabled(FName PluginId) const;
    bool OwnsFeature(FName FeatureId) const;
    UPROPERTY(Transient) FDivineBeastsDemoMenuSnapshot Snapshot;
    /** 订阅一次/失活撤销；重绑当前World时先解绑旧World。 */
    void Subscribe();
    void DetachSubscriptions();
    /** 目录/运行事件统一重建投影，事件来源已绑定作用域，不使用业务Tick。 */
    void RebuildSnapshot();
    void OnRunSnapshotChanged(const FDivineBeastsDemoRunSnapshot& Run);
};
