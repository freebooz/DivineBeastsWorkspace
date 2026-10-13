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

    /** 现有开发地图可由多个插件复用；关联身份不是各项功能已通过验收。
     * 这里只承认真实地图路径，不创建新的空关卡或虚假插件身份。
     */
    TArray<FName> ResolveScenarioPluginIds(const FString& PackagePath)
    {
        if (PackagePath == FDivineBeastsPluginDemoMode::GetHubMapPackage()) return {};
        if (PackagePath.EndsWith(TEXT("/L_GP_CombatReview")))
            return {FName(TEXT("GamePlatformCombat")), FName(TEXT("GamePlatformAbilitySystem")), FName(TEXT("DBAGameplay"))};
        if (PackagePath.EndsWith(TEXT("/L_GP_InteractionAIReview")))
            return {FName(TEXT("GamePlatformAI")), FName(TEXT("GamePlatformInteraction")),
                    FName(TEXT("GamePlatformNavigation")), FName(TEXT("GamePlatformQuest"))};
        if (PackagePath.EndsWith(TEXT("/L_DBA_MainArenaReview")))
            return {FName(TEXT("GamePlatformArena")), FName(TEXT("DBAArena")), FName(TEXT("MobaPresentation"))};
        if (PackagePath.EndsWith(TEXT("/L_GP_SFXReview")))
            return {FName(TEXT("GamePlatformSFX")), FName(TEXT("DBASFXPack_Core"))};
        if (PackagePath.EndsWith(TEXT("/L_DBA_HeroShowcase")))
            return {FName(TEXT("GamePlatformCharacter")), FName(TEXT("DBAGameplay")),
                    FName(TEXT("DBAContentPack_Common")), FName(TEXT("DBAHeroPack_Rat")),
                    FName(TEXT("DBAHeroPack_Ox")), FName(TEXT("DBAHeroPack_Tiger")),
                    FName(TEXT("DBAHeroPack_Rabbit")), FName(TEXT("DBAHeroPack_Dragon")),
                    FName(TEXT("DBAHeroPack_Snake")), FName(TEXT("DBAHeroPack_Horse")),
                    FName(TEXT("DBAHeroPack_Goat")), FName(TEXT("DBAHeroPack_Monkey")),
                    FName(TEXT("DBAHeroPack_Rooster")), FName(TEXT("DBAHeroPack_Dog")),
                    FName(TEXT("DBAHeroPack_Boar"))};
        if (PackagePath.StartsWith(TEXT("/Game/Development/Weather/")))
            return {FName(TEXT("GamePlatformWeather"))};
        if (PackagePath.StartsWith(TEXT("/Game/Development/MossSnow/")) ||
            PackagePath.StartsWith(TEXT("/Game/Development/Snow/")))
            return {FName(TEXT("GamePlatformSurface")), FName(TEXT("GamePlatformWeather"))};
        if (PackagePath.StartsWith(TEXT("/Game/Development/Terrain/")))
            return {FName(TEXT("GamePlatformSurface"))};
        if (PackagePath.StartsWith(TEXT("/Game/Development/BirdFlock/")) ||
            PackagePath.StartsWith(TEXT("/Game/Development/FallingFoliage/")) ||
            PackagePath.StartsWith(TEXT("/Game/Development/Fauna/")))
            return {FName(TEXT("GamePlatformVFX"))};
        if (PackagePath.Contains(TEXT("/PCG/")))
            return {FName(TEXT("GamePlatformPCG"))};
        return {FName(TEXT("GamePlatformDeveloperTools"))};
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
        const TArray<FName> Owners = DivineBeasts::Demo::ResolveScenarioPluginIds(Package);
        const bool bSpecializedReview = Package.StartsWith(
            TEXT("/Game/Development/Foundation/PluginReviews/Maps/"));
        for (const FName PluginId : Owners)
        {
            FDivineBeastsDemoPlugin* PluginEntry = Plugins.FindByPredicate(
                [PluginId](const FDivineBeastsDemoPlugin& Candidate)
                { return Candidate.PluginId == PluginId; });
            // 插件发现必须来自真实.uplugin；未知归属不能制造假的可选入口。
            if (!PluginEntry) continue;
            FDivineBeastsDemoScenario Item;
            // 同图多插件分别登记，避免相同ScenarioId导致其他插件的错误启用状态。
            Item.ScenarioId = FName(*(PluginId.ToString() + TEXT("::") + Package));
            Item.PluginId = PluginId;
            Item.Title = FText::FromString(Asset.AssetName.ToString());
            Item.MapAsset = Asset.ToSoftObjectPath();
            Item.bDiagnosticOnly = Package.Contains(TEXT("/Probes/")) ||
                Package.Contains(TEXT("CaptureBackup")) ||
                (Package.Contains(TEXT("/Foundation/")) && !bSpecializedReview);
            Item.VerificationStatus = FText::FromString(
                Item.bDiagnosticOnly ? TEXT("技术诊断关卡，非正式展示") :
                TEXT("已发现真实关卡；实际运行和人工审核待验证"));
            PluginEntry->DiscoveredMapCount++;
            Item.bAvailable = PluginEntry->bEnabled;
            Scenarios.Add(MoveTemp(Item));
        }
    }
    Scenarios.Sort([](const FDivineBeastsDemoScenario& A, const FDivineBeastsDemoScenario& B)
    {
        return A.ScenarioId.ToString() < B.ScenarioId.ToString();
    });
    UE_LOG(LogTemp, Display, TEXT("神兽联盟演示目录发现插件=%d，演示入口=%d；均待实际运行验收。"),
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
    // SceneId是插件+地图唯一入口，真正切换只允许已登记且存在的开发地图。
    const FString MapPackage = Item->MapAsset.GetLongPackageName();
    if (!MapPackage.StartsWith(TEXT("/Game/Development/")) ||
        MapPackage == FDivineBeastsPluginDemoMode::GetHubMapPackage() ||
        !FPackageName::DoesPackageExist(MapPackage) || !GetWorld())
    {
        LastOperationMessage = FText::FromString(TEXT("地图没有可加载的真实资产，拒绝进入。"));
        return false;
    }
    LastOperationMessage = FText::FromString(TEXT("已提交地图跳转请求，尚待UE运行和人工验收。"));
    UGameplayStatics::OpenLevel(GetWorld(), FName(*MapPackage));
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
