// 演示UI蓝图的项目层纯桥接基类；视觉布局和交互事件须由Monolith MCP制作。
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "DivineBeastsDemoHubWidget.generated.h"

/** WBP_DBA_PluginDemoHub等真实UI蓝图的父类；不在C++构建另一套可视控件树。 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoHubWidget : public UUserWidget
{
    GENERATED_BODY()
public:
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
    UDivineBeastsDemoCatalogSubsystem* GetCatalog() const;
};
