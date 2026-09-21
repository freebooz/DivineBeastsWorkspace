#include "Bootstrap/DBAFoundationCoordinator.h"
#include "Bootstrap/DBAFoundationHUD.h"
#include "Bootstrap/DBAFoundationPolicy.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAFoundation, Log, All);

void UDBAFoundationCoordinator::Initialize(UGameInstance& Owner)
{
    Shutdown();
#if !UE_BUILD_SHIPPING
    if (DBA::Foundation::ResolveMode(!UE_BUILD_SHIPPING,
        FParse::Param(FCommandLine::Get(), TEXT("FoundationStandalone")),
        IsRunningCommandlet(), IsRunningDedicatedServer()) == DBA::Foundation::EMode::Disabled) { return; }
    OwnerInstance = &Owner;
    FGuid ParsedRunId;
    if (!FParse::Value(FCommandLine::Get(), TEXT("FoundationRunId="), RunId) || !FGuid::Parse(RunId, ParsedRunId))
    {
        Diagnostics = TEXT("基础工程开发验证：缺少有效 FoundationRunId，拒绝启动");
        UE_LOG(LogDBAFoundation, Error, TEXT("%s"), *Diagnostics);
        return;
    }
    Diagnostics = TEXT("基础工程开发验证：等待本实例世界就绪");
    // 低频实例级轮询等待可操作世界；切图不借用旧世界计时器，不假造固定延时成功。
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UDBAFoundationCoordinator::Tick), 0.1f);
#endif
}

bool UDBAFoundationCoordinator::Tick(float)
{
    UGameInstance* Instance = OwnerInstance.Get();
    if (!Instance) { return false; }
    UWorld* World = Instance->GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || World->GetGameInstance() != Instance) { return true; }
    const FString Map = World->GetOutermost()->GetName();
    if (World->GetNetMode() == NM_DedicatedServer)
    {
        if (ReportedWorld.Get() != World)
        {
            UE_LOG(LogDBAFoundation, Display, TEXT("FoundationServerReady RunId=%s Map=%s"), *RunId, *Map);
            ReportedWorld = World;
        }
        Diagnostics = TEXT("基础专用服务器：世界就绪，不创建玩家HUD或主流程");
        return true;
    }
    bool bHasLocalController = false;
    for (ULocalPlayer* Player : Instance->GetLocalPlayers())
    {
        APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
        if (!Controller || !Controller->IsLocalController()) { continue; }
        bHasLocalController = true;
        if (!Cast<ADBAFoundationHUD>(Controller->GetHUD())) { Controller->ClientSetHUD(ADBAFoundationHUD::StaticClass()); }
    }
    Diagnostics = FString::Printf(TEXT("基础工程开发验证\n地图：%s\n运行：%s\n薄宿主：%s\nM0完整流程：尚未接入"),
        *Map, *RunId, bHasLocalController ? TEXT("世界与本地观察者就绪") : TEXT("等待本地观察者"));
    if (bHasLocalController && ReportedWorld.Get() != World)
    {
        UE_LOG(LogDBAFoundation, Display, TEXT("FoundationHostReady RunId=%s Map=%s"), *RunId, *Map);
        ReportedWorld = World;
    }
    return true;
}

void UDBAFoundationCoordinator::Shutdown()
{
    if (TickerHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle); TickerHandle.Reset(); }
    OwnerInstance.Reset();
    ReportedWorld.Reset();
    Diagnostics = TEXT("基础工程开发验证未启用或已关闭");
}
