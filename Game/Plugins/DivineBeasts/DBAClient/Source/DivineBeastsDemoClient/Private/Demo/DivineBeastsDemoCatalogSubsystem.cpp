// UE真实插件描述＋开发地图注册表的开发会话目录：不使用伪资源、登录或另一资产管理器。
// 所有枚举只在GameInstance初始化/手动刷新发生，任何Map请求必须在场景候选表中。
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace DivineBeasts::Demo
{
    /** 将被发现的项目插件路径转换为三层/内容包身份，外部Engine插件不纳入。 */
    bool ResolveLayer(const FString& RelativeDir, EDivineBeastsDemoLayer& OutLayer)
    {
        if (RelativeDir.StartsWith(TEXT("DivineBeasts/ContentPacks/"), ESearchCase::IgnoreCase))
        {
            OutLayer = EDivineBeastsDemoLayer::Content; return true;
        }
        if (RelativeDir.StartsWith(TEXT("DivineBeasts/"), ESearchCase::IgnoreCase))
        {
            OutLayer = EDivineBeastsDemoLayer::Project; return true;
        }
        if (RelativeDir.StartsWith(TEXT("MobaCommon/"), ESearchCase::IgnoreCase))
        {
            OutLayer = EDivineBeastsDemoLayer::Moba; return true;
        }
        if (RelativeDir.StartsWith(TEXT("GamePlatform/"), ESearchCase::IgnoreCase))
        {
            OutLayer = EDivineBeastsDemoLayer::Platform; return true;
        }
        return false;
    }

    /** 这是开发地图的所有权候选分类，而非已经验收的单项业务功能身份。 */
    FName GuessOwningPlugin(const FString& PackagePath)
    {
        if (PackagePath.StartsWith(TEXT("/Game/Development/Weather/"))) return TEXT("GamePlatformWeather");
        if (PackagePath.StartsWith(TEXT("/Game/Development/MossSnow/")) ||
            PackagePath.StartsWith(TEXT("/Game/Development/Snow/"))) return TEXT("GamePlatformSurface");
        if (PackagePath.StartsWith(TEXT("/Game/Development/Terrain/"))) return TEXT("GamePlatformSurface");
        if (PackagePath.StartsWith(TEXT("/Game/Development/BirdFlock/")) ||
            PackagePath.StartsWith(TEXT("/Game/Development/FallingFoliage/")) ||
            PackagePath.StartsWith(TEXT("/Game/Development/Fauna/"))) return TEXT("GamePlatformVFX");
        if (PackagePath.Contains(TEXT("/PCG/"))) return TEXT("GamePlatformPCG");
        return TEXT("GamePlatformDeveloperTools");
    }
}

bool UDivineBeastsDemoCatalogSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return Super::ShouldCreateSubsystem(Outer) && FDivineBeastsPluginDemoMode::IsEnabled();
}

void UDivineBeastsDemoCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RefreshCatalog();
}

void UDivineBeastsDemoCatalogSubsystem::Deinitialize()
{
    Plugins.Reset();
    Scenarios.Reset();
    LastOperationMessage = FText::GetEmpty();
    Super::Deinitialize();
}

void UDivineBeastsDemoCatalogSubsystem::RefreshCatalog()
{
    Plugins.Reset();
    Scenarios.Reset();
    LastOperationMessage = FText::GetEmpty();
    if (!FDivineBeastsPluginDemoMode::IsEnabled())
    {
        LastOperationMessage = FText::FromString(TEXT("非插件演示模式，拒绝刷新。"));
        return;
    }

    FString PluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
    FPaths::MakeStandardFilename(PluginsDir);
    PluginsDir.RemoveFromEnd(TEXT("/"));
    const FString Prefix = PluginsDir + TEXT("/");
    TSet<FName> Seen;
    for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
    {
        FString Root = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
        FPaths::MakeStandardFilename(Root);
        if (!Root.StartsWith(Prefix, ESearchCase::IgnoreCase)) continue;
        const FString Relative = Root.RightChop(Prefix.Len());
        EDivineBeastsDemoLayer Layer;
        if (!DivineBeasts::Demo::ResolveLayer(Relative, Layer)) continue;
        const FName Id(*Plugin->GetName());
        if (Seen.Contains(Id))
        {
            UE_LOG(LogTemp, Error, TEXT("演示插件目录重复身份：%s"), *Id.ToString());
            continue;
        }
        Seen.Add(Id);
        FDivineBeastsDemoPlugin Item;
        Item.PluginId = Id;
        const FString FriendlyName = Plugin->GetDescriptor().FriendlyName;
        Item.DisplayName = FText::FromString(FriendlyName.IsEmpty() ? Id.ToString() : FriendlyName);
        Item.Description = Plugin->GetDescriptor().Description;
        Item.Layer = Layer;
        Item.bEnabled = Plugin->IsEnabled();
        Plugins.Add(MoveTemp(Item));
    }
    Plugins.Sort([](const FDivineBeastsDemoPlugin& A, const FDivineBeastsDemoPlugin& B)
    {
        return A.PluginId.ToString() < B.PluginId.ToString();
    });

    // AssetRegistry是开发关卡候选的唯一来源；不以文件系统存在冒充UWorld加载成功。
    FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> DiscoveredAssets;
    RegistryModule.Get().GetAssetsByPath(FName(TEXT("/Game/Development")), DiscoveredAssets, true);
    TSet<FName> KnownScenarios;
    for (const FAssetData& Asset : DiscoveredAssets)
    {
        if (Asset.AssetClassPath != UWorld::StaticClass()->GetClassPathName()) continue;
        const FString Package = Asset.PackageName.ToString();
        if (!Package.StartsWith(TEXT("/Game/Development/"))) continue;
        if (KnownScenarios.Contains(Asset.PackageName)) continue;
        KnownScenarios.Add(Asset.PackageName);
        FDivineBeastsDemoScenario Item;
        Item.ScenarioId = Asset.PackageName;
        Item.PluginId = DivineBeasts::Demo::GuessOwningPlugin(Package);
        Item.Title = FText::FromString(Asset.AssetName.ToString());
        Item.MapAsset = Asset.ToSoftObjectPath();
        Item.bDiagnosticOnly = Package.Contains(TEXT("/Probes/")) ||
            Package.Contains(TEXT("CaptureBackup")) ||
            Package.Contains(TEXT("/Foundation/"));
        Item.VerificationStatus = FText::FromString(
            Item.bDiagnosticOnly ? TEXT("技术诊断关卡，非正式展示") : TEXT("已发现真实关卡；实际运行和人工审核待验证"));
        for (FDivineBeastsDemoPlugin& Plugin : Plugins)
        {
            if (Plugin.PluginId == Item.PluginId)
            {
                Plugin.DiscoveredMapCount++;
                Item.bAvailable = Plugin.bEnabled;
                break;
            }
        }
        Scenarios.Add(MoveTemp(Item));
    }
    Scenarios.Sort([](const FDivineBeastsDemoScenario& A, const FDivineBeastsDemoScenario& B)
    {
        return A.ScenarioId.ToString() < B.ScenarioId.ToString();
    });
    UE_LOG(LogTemp, Display, TEXT("神兽联盟演示目录发现插件=%d，开发关卡=%d；均待实际运行验收。"),
        Plugins.Num(), Scenarios.Num());
}

TArray<FDivineBeastsDemoScenario> UDivineBeastsDemoCatalogSubsystem::GetScenarios(FName PluginId) const
{
    TArray<FDivineBeastsDemoScenario> Result;
    for (const FDivineBeastsDemoScenario& Item : Scenarios)
    {
        if (Item.PluginId == PluginId) Result.Add(Item);
    }
    return Result;
}

bool UDivineBeastsDemoCatalogSubsystem::OpenScenario(FName ScenarioId)
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled()) return false;
    const FDivineBeastsDemoScenario* Item = Scenarios.FindByPredicate(
        [ScenarioId](const FDivineBeastsDemoScenario& Value) { return Value.ScenarioId == ScenarioId; });
    if (!Item || !Item->bAvailable)
    {
        LastOperationMessage = FText::FromString(TEXT("演示项目未登记、对应插件未启用或资源不可用。"));
        return false;
    }
    if (!FPackageName::DoesPackageExist(Item->ScenarioId.ToString()) || !GetWorld())
    {
        LastOperationMessage = FText::FromString(TEXT("地图没有可加载的真实资产，拒绝进入。"));
        return false;
    }
    LastOperationMessage = FText::FromString(TEXT("已提交地图跳转请求，尚待UE运行和人工验收。"));
    UGameplayStatics::OpenLevel(GetWorld(), Item->ScenarioId);
    return true;
}

bool UDivineBeastsDemoCatalogSubsystem::ReturnToHub()
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled() || !GetWorld()) return false;
    const FString Map = FDivineBeastsPluginDemoMode::GetHubMapPackage();
    if (!FPackageName::DoesPackageExist(Map))
    {
        LastOperationMessage = FText::FromString(TEXT("插件演示总览关卡尚未真实生成。"));
        return false;
    }
    LastOperationMessage = FText::FromString(TEXT("已请求返回演示总览。"));
    UGameplayStatics::OpenLevel(GetWorld(), FName(*Map));
    return true;
}
