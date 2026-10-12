// 插件演示世界子系统：鸟群/天气复用正式公开接口；UI通过GamePlatformData中央租约异步装载。
// 未加载的真实UI蓝图不产生假控件，也不将资产缺失误报为成功。
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "UI/DivineBeastsDemoHubWidget.h"
#include "Environment/GamePlatformBirdFlockActor.h"
#include "Weather/DivineBeastsWeatherReviewController.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "UObject/SoftObjectPath.h"

namespace DivineBeastsDemo
{
    /** 只在明确的项目开发地图自动挂载演示界面，不覆盖正式游戏关卡。 */
    const TCHAR* HubWidgetClass = TEXT("/Game/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub.WBP_DBA_PluginDemoHub_C");
    const TCHAR* OverlayWidgetClass = TEXT("/Game/Development/PluginDemos/UI/WBP_DBA_PluginDemoOverlay.WBP_DBA_PluginDemoOverlay_C");
}

bool UDivineBeastsDemoWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && FDivineBeastsPluginDemoMode::IsEnabled()
        && World && World->IsGameWorld();
}

void UDivineBeastsDemoWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    SourceWorld = &InWorld;
    // 开发模式也可能预览正式资产：只允许在专用总览或显式已登记关卡中加载控制台。
    if (UGameInstance* Instance = InWorld.GetGameInstance())
    {
        const auto* Catalog = Instance->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>();
        const FName CurrentMap(*UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName()));
        const FName Hub(FDivineBeastsPluginDemoMode::GetHubMapPackage());
        bool bRegisteredMap = CurrentMap == Hub;
        if (Catalog && !bRegisteredMap)
        {
            for (const FDivineBeastsDemoPlugin& Plugin : Catalog->GetPlugins())
            {
                for (const FDivineBeastsDemoScenario& Scenario : Catalog->GetScenarios(Plugin.PluginId))
                {
                    if (Scenario.ScenarioId == CurrentMap && Scenario.bAvailable)
                    {
                        bRegisteredMap = true;
                        break;
                    }
                }
                if (bRegisteredMap) break;
            }
        }
        if (bRegisteredMap)
        {
            RequestDemoOverlay(InWorld);
        }
    }
}

void UDivineBeastsDemoWorldSubsystem::Deinitialize()
{
    ResetDemoChanges();
    ReleaseDemoOverlay();
    OriginalCounts.Reset();
    OriginalQuality.Reset();
    OriginalWeatherActor.Reset();
    SourceWorld.Reset();
    Super::Deinitialize();
}

void UDivineBeastsDemoWorldSubsystem::RequestDemoOverlay(UWorld& World)
{
    UGameInstance* Instance = World.GetGameInstance();
    IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data)
    {
        LastFeedback = FText::FromString(TEXT("平台Data资源服务不可用；不能启动真实演示UI。"));
        return;
    }
    const FString CurrentPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
    const FString ClassPath = CurrentPackage == FDivineBeastsPluginDemoMode::GetHubMapPackage()
        ? DivineBeastsDemo::HubWidgetClass : DivineBeastsDemo::OverlayWidgetClass;
    const FSoftObjectPath WidgetClassPath(ClassPath);
    FGamePlatformResult Requested;
    TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> WeakThis(this);
    UILease = Data->AcquireResources(
        {WidgetClassPath}, EGamePlatformDataLifetime::World, this,
        [WeakThis, WidgetClassPath](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
        {
            if (UDivineBeastsDemoWorldSubsystem* Self = WeakThis.Get())
            {
                Self->OnOverlayAssetReady(WidgetClassPath, Lease, Result.IsSuccess());
            }
        }, Requested);
    if (!Requested.IsSuccess() || !UILease.IsValid())
    {
        LastFeedback = FText::FromString(TEXT("演示Widget加载请求未被Data服务接受。请检查真实WBP资源。"));
    }
}

void UDivineBeastsDemoWorldSubsystem::OnOverlayAssetReady(
    FSoftObjectPath WidgetClassPath, const FGamePlatformDataLease& Lease, bool bSucceeded)
{
    UWorld* World = GetWorld();
    if (!bSucceeded || !World || World != SourceWorld.Get() || World->bIsTearingDown ||
        Lease.LeaseId != UILease.LeaseId)
    {
        LastFeedback = FText::FromString(TEXT("演示UI资源加载失败、已取消或所在世界已退出。"));
        return;
    }
    const UClass* LoadedClass = Cast<UClass>(WidgetClassPath.ResolveObject());
    if (!LoadedClass || !LoadedClass->IsChildOf(UDivineBeastsDemoHubWidget::StaticClass()))
    {
        LastFeedback = FText::FromString(TEXT("演示UI尚未由Monolith生成，或蓝图父类不是UDivineBeastsDemoHubWidget。"));
        return;
    }
    UGameInstance* Instance = World->GetGameInstance();
    ULocalPlayer* Player = Instance ? Instance->GetFirstGamePlayer() : nullptr;
    APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
    if (!Controller)
    {
        LastFeedback = FText::FromString(TEXT("尚无本地玩家控制器，不能创建演示UI。"));
        return;
    }
    UDivineBeastsDemoHubWidget* Widget = CreateWidget<UDivineBeastsDemoHubWidget>(
        Controller, const_cast<UClass*>(LoadedClass));
    if (!Widget)
    {
        LastFeedback = FText::FromString(TEXT("演示Widget创建失败。"));
        return;
    }
    ActiveWidget = Widget;
    Widget->AddToViewport(100);
    LastFeedback = FText::FromString(TEXT("真实演示UI已加载，功能仍需逐项人工验收。"));
}

void UDivineBeastsDemoWorldSubsystem::ReleaseDemoOverlay()
{
    if (UDivineBeastsDemoHubWidget* Widget = ActiveWidget.Get())
    {
        Widget->RemoveFromParent();
    }
    ActiveWidget.Reset();
    if (FGamePlatformDataLease Lease = UILease; Lease.IsValid())
    {
        UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        if (IGamePlatformDataService* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr)
        {
            Data->ReleaseResources(Lease);
        }
    }
    UILease = {};
}

int32 UDivineBeastsDemoWorldSubsystem::GetBirdActorCount() const
{
    UWorld* World = GetWorld();
    if (!World) return 0;
    int32 Count = 0;
    for (TActorIterator<AGamePlatformBirdFlockActor> It(World); It; ++It) ++Count;
    return Count;
}

int32 UDivineBeastsDemoWorldSubsystem::GetActualBirdCount() const
{
    UWorld* World = GetWorld();
    if (!World) return 0;
    int32 Count = 0;
    for (TActorIterator<AGamePlatformBirdFlockActor> It(World); It; ++It)
    {
        Count += It->GetEffectiveBirdCount();
    }
    return Count;
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdCount(int32 Count)
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled() || Count < 0 || Count > 512)
    {
        LastFeedback = FText::FromString(TEXT("鸟群数量非法，范围为0至512。"));
        return false;
    }
    UWorld* World = GetWorld();
    if (!World) return false;
    int32 Changed = 0;
    for (TActorIterator<AGamePlatformBirdFlockActor> It(World); It; ++It)
    {
        AGamePlatformBirdFlockActor* Bird = *It;
        if (!OriginalCounts.Contains(Bird)) OriginalCounts.Add(Bird, Bird->BirdCountOverride);
        if (Bird->SetBirdCount(Count)) ++Changed;
    }
    LastFeedback = FText::FromString(
        Changed > 0 ? FString::Printf(TEXT("已修改%d个真实鸟群Actor，当前可见鸟数%d。"), Changed, GetActualBirdCount())
                    : TEXT("当前地图没有可调整的真实鸟群Actor，或请求被VFX插件拒绝。"));
    return Changed > 0;
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdQuality(float Fraction)
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled() || !FMath::IsFinite(Fraction) || Fraction < 0.f || Fraction > 1.f)
    {
        LastFeedback = FText::FromString(TEXT("鸟群画质参数非法，范围为0至1。"));
        return false;
    }
    UWorld* World = GetWorld();
    if (!World) return false;
    int32 Changed = 0;
    for (TActorIterator<AGamePlatformBirdFlockActor> It(World); It; ++It)
    {
        AGamePlatformBirdFlockActor* Bird = *It;
        if (!OriginalQuality.Contains(Bird)) OriginalQuality.Add(Bird, Bird->QualityFraction);
        if (Bird->SetQualityFraction(Fraction)) ++Changed;
    }
    LastFeedback = FText::FromString(Changed > 0
        ? FString::Printf(TEXT("已更新%d个鸟群Actor画质比例，当前可见%d只鸟。"), Changed, GetActualBirdCount())
        : TEXT("当前地图没有可调整的真实鸟群Actor或操作被拒绝。"));
    return Changed > 0;
}

bool UDivineBeastsDemoWorldSubsystem::ApplyWeather(EGamePlatformWeatherType Weather, float TransitionSeconds)
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled() || !FMath::IsFinite(TransitionSeconds) ||
        TransitionSeconds < 0.f || TransitionSeconds > 60.f)
    {
        LastFeedback = FText::FromString(TEXT("天气过渡秒数非法，范围为0至60秒。"));
        return false;
    }
    UWorld* World = GetWorld();
    if (!World) return false;
    for (TActorIterator<ADivineBeastsWeatherReviewController> It(World); It; ++It)
    {
        ADivineBeastsWeatherReviewController* Controller = *It;
        if (!bHasOriginalWeather)
        {
            FGamePlatformWeatherState Current;
            if (!Controller->ReadCurrentWeather(Current))
            {
                LastFeedback = FText::FromString(TEXT("读取真实天气状态失败。"));
                return false;
            }
            OriginalWeatherActor = Controller;
            OriginalWeatherType = Current.Type;
            bHasOriginalWeather = true;
        }
        if (!Controller->ApplyReviewWeather(Weather, TransitionSeconds))
        {
            LastFeedback = FText::FromString(TEXT("平台天气拒绝当前World的非权威操作，不会伪造天气。"));
            return false;
        }
        LastFeedback = FText::FromString(TEXT("已向真实天气审核Actor提交变化。需验证粒子、声音和过渡效果。"));
        return true;
    }
    LastFeedback = FText::FromString(TEXT("当前地图没有天气审核控制Actor。"));
    return false;
}

bool UDivineBeastsDemoWorldSubsystem::ResetDemoChanges()
{
    bool bAllSucceeded = true;
    for (const auto& Entry : OriginalCounts)
    {
        if (AGamePlatformBirdFlockActor* Bird = Entry.Key.Get())
        {
            bAllSucceeded &= Bird->SetBirdCount(Entry.Value);
        }
    }
    for (const auto& Entry : OriginalQuality)
    {
        if (AGamePlatformBirdFlockActor* Bird = Entry.Key.Get())
        {
            bAllSucceeded &= Bird->SetQualityFraction(Entry.Value);
        }
    }
    OriginalCounts.Reset();
    OriginalQuality.Reset();
    if (bHasOriginalWeather)
    {
        if (ADivineBeastsWeatherReviewController* WeatherActor = OriginalWeatherActor.Get())
        {
            bAllSucceeded &= WeatherActor->ApplyReviewWeather(OriginalWeatherType, 0.f);
        }
        else bAllSucceeded = false;
    }
    bHasOriginalWeather = false;
    OriginalWeatherActor.Reset();
    LastFeedback = FText::FromString(bAllSucceeded
        ? TEXT("演示参数已向真实组件提交复原。")
        : TEXT("部分世界资源已失效或拒绝复原，请重新进入审核关卡。"));
    return bAllSucceeded;
}
