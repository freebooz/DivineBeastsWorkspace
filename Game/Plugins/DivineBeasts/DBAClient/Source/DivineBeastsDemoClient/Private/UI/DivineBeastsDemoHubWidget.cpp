// 项目层UI适配：读取演示目录快照并转发操作，无URL、账号或业务网络请求。
#include "UI/DivineBeastsDemoHubWidget.h"
#include "Engine/GameInstance.h"

UDivineBeastsDemoCatalogSubsystem* UDivineBeastsDemoHubWidget::GetCatalog() const
{
    UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>() : nullptr;
}

TArray<FDivineBeastsDemoPlugin> UDivineBeastsDemoHubWidget::GetDiscoveredPlugins() const
{
    const auto* Catalog = GetCatalog();
    return Catalog ? Catalog->GetPlugins() : TArray<FDivineBeastsDemoPlugin>();
}

TArray<FDivineBeastsDemoScenario> UDivineBeastsDemoHubWidget::GetPluginScenarios(FName PluginId) const
{
    const auto* Catalog = GetCatalog();
    return Catalog ? Catalog->GetScenarios(PluginId) : TArray<FDivineBeastsDemoScenario>();
}

bool UDivineBeastsDemoHubWidget::RequestOpenScenario(FName ScenarioId)
{
    auto* Catalog = GetCatalog();
    return Catalog && Catalog->OpenScenario(ScenarioId);
}

bool UDivineBeastsDemoHubWidget::RequestReturnToHub()
{
    auto* Catalog = GetCatalog();
    return Catalog && Catalog->ReturnToHub();
}

FText UDivineBeastsDemoHubWidget::GetDemoOperationMessage() const
{
    const auto* Catalog = GetCatalog();
    return Catalog ? Catalog->GetLastOperationMessage()
        : FText::FromString(TEXT("演示目录未初始化或当前不是受控开发客户端。"));
}
