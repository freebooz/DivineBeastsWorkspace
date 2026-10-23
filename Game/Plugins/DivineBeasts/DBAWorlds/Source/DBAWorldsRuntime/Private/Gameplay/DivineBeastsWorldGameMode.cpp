// 项目共享GameMode只指定项目控制器；认证和生成仍由平台唯一门禁拥有。
#include "Gameplay/DivineBeastsWorldGameMode.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Definitions/GamePlatformWeatherPresetDefinition.h"
#include "Types/GamePlatformId.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#endif

ADivineBeastsWorldGameMode::ADivineBeastsWorldGameMode()
{
    PlayerControllerClass = ADivineBeastsWorldPlayerController::StaticClass();
}

void ADivineBeastsWorldGameMode::BeginPlay()
{
    Super::BeginPlay();
    bWeatherShuttingDown = false;
    UWorld* World = GetWorld();
    if (!HasAuthority() || !IsValid(World) || World->bIsTearingDown) return;

    UGamePlatformWeatherWorldSubsystem* Weather = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    if (!Weather || !Weather->ActivateWeather(InitialWeather))
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟世界天气初始化失败：服务器模式、初始天气或世界实例非法。"));
        return;
    }
#if !UE_BUILD_SHIPPING
    // 仅当测试操作者显式传入两个开发参数，并且真正运行Village Dedicated Server时，
    // 才使用固定序列验证天气复制。不能让客户端、普通服务器或Shipping隐式启用。
    bVillageGroundSnowReview = FParse::Param(FCommandLine::Get(), TEXT("DBAVillageGroundSnowReviewSchedule"));
    if (bVillageGroundSnowReview || FParse::Param(FCommandLine::Get(), TEXT("DBAVillageSnowReviewSchedule")))
    {
        const bool bDedicatedVillage =
            World->GetNetMode() == NM_DedicatedServer &&
            World->GetOutermost()->GetName() ==
                TEXT("/DBAWorldPack_Village/Maps/L_Village_Start");
        const bool bUnattendedReview =
            FParse::Param(FCommandLine::Get(), TEXT("unattended"));
        if (bDedicatedVillage && bUnattendedReview &&
            !bEnableWeatherSchedule && InitialWeatherPresetId.IsNone())
        {
            VillageSnowReviewStep = 0;
            VillageSnowReviewJoinedPlayers = 0;
            bVillageSnowReviewArmed = true;
            bVillageSnowReviewStarted = false;
            UE_LOG(LogTemp, Display,
                TEXT("VILLAGE_SNOW_REVIEW_ARMED：仅本次Village专服等待两名成功PostLogin客户端，之后执行固定小雪/大雪/晴/大雪序列。"));
            return;
        }
        UE_LOG(LogTemp, Error,
            TEXT("VILLAGE_SNOW_REVIEW_REFUSED：不是显式无人值守Village专服，或已有正式调度/预设。"));
    }
#endif


    // 自动调度与单份初始天气预设互斥：避免Data异步回调取消已启动的权威调度。
    if (bEnableWeatherSchedule)
    {
        if (!Weather->ConfigureSchedule(WeatherSchedule, static_cast<int32>(World->GetUniqueID())) ||
            !Weather->StartSchedule())
        {
            UE_LOG(LogTemp, Warning, TEXT("神兽联盟自动天气表无效，保持初始化天气；检查时长、权重和状态。"));
        }
        if (!InitialWeatherPresetId.IsNone())
        {
            UE_LOG(LogTemp, Warning, TEXT("神兽联盟世界同时配置自动天气和初始天气预设；自动调度优先，预设不加载。"));
        }
        return;
    }

    if (!InitialWeatherPresetId.IsNone())
    {
        BeginLoadInitialWeatherPreset();
    }
}

void ADivineBeastsWorldGameMode::BeginLoadInitialWeatherPreset()
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (bWeatherShuttingDown || !HasAuthority() || !Data) 
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟天气预设加载未启动：缺少服务器权威或平台数据服务。"));
        return;
    }

    FGamePlatformId LogicalId;
    if (!FGamePlatformId::TryParse(InitialWeatherPresetId.ToString(), LogicalId))
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟世界的初始天气逻辑ID非法：%s。"), *InitialWeatherPresetId.ToString());
        return;
    }
    const FName CanonicalId(*LogicalId.ToString());
    const FPrimaryAssetId PrimaryId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), CanonicalId);
    FGamePlatformResult Accepted;
    const TWeakObjectPtr<ADivineBeastsWorldGameMode> WeakThis(this);
    InitialWeatherPresetLease = Data->AcquireDefinition(
        PrimaryId,
        UGamePlatformWeatherPresetDefinition::StaticClass(),
        {},
        EGamePlatformDataLifetime::World,
        TWeakObjectPtr<UObject>(this),
        [WeakThis](const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        {
            if (ADivineBeastsWorldGameMode* Self = WeakThis.Get())
            {
                Self->HandleInitialWeatherPresetCompleted(CompletedLease, Result);
            }
        },
        Accepted);
    if (!Accepted.IsSuccess() || !InitialWeatherPresetLease.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟世界初始天气预设未被平台Data服务接纳：%s。"), *PrimaryId.ToString());
    }
}

void ADivineBeastsWorldGameMode::HandleInitialWeatherPresetCompleted(
    const FGamePlatformDataLease& CompletedLease,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (bWeatherShuttingDown || !HasAuthority() || !InitialWeatherPresetLease.IsValid() ||
        CompletedLease.LeaseId != InitialWeatherPresetLease.LeaseId ||
        CompletedLease.Generation != InitialWeatherPresetLease.Generation ||
        CompletedLease.ScopeId != InitialWeatherPresetLease.ScopeId ||
        CompletedLease.IssuerProof != InitialWeatherPresetLease.IssuerProof)
    {
        return; // 旧世界、旧代次或伪造句柄的异步回调不得覆盖当前权威天气。
    }

    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    IGamePlatformDataService* Data = IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr;
    const UGamePlatformWeatherPresetDefinition* Preset = Data && Result.IsSuccess() &&
        Data->GetLeaseState(InitialWeatherPresetLease) == EGamePlatformDataRequestState::Succeeded
            ? Cast<UGamePlatformWeatherPresetDefinition>(Data->GetLoadedDefinition(InitialWeatherPresetLease))
            : nullptr;
    UGamePlatformWeatherWorldSubsystem* Weather = IsValid(World)
        ? World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>() : nullptr;
    if (!IsValid(Preset) || !Weather || !Weather->ApplyPreset(Preset))
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟天气预设加载或应用失败，继续保持原初始天气；ID=%s。"),
            *InitialWeatherPresetId.ToString());
    }
    // 当前预设已投影为服务器状态数值，不再需要持有数据资产整个世界期限。
    if (Data) Data->ReleaseDefinition(InitialWeatherPresetLease);
    InitialWeatherPresetLease = {};
}



#if !UE_BUILD_SHIPPING
void ADivineBeastsWorldGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UWorld* World = GetWorld();
    if (!bVillageSnowReviewArmed || bVillageSnowReviewStarted || !IsValid(World) ||
        !IsValid(NewPlayer) || World->bIsTearingDown ||
        World->GetNetMode() != NM_DedicatedServer)
    {
        return;
    }
    ++VillageSnowReviewJoinedPlayers;
    UE_LOG(LogTemp, Display,
        TEXT("VILLAGE_SNOW_REVIEW_CLIENT_READY：经过GameMode正常PostLogin的客户端计数=%d。"),
        VillageSnowReviewJoinedPlayers);
    if (VillageSnowReviewJoinedPlayers >= 2)
    {
        bVillageSnowReviewStarted = true;
        // 接受两位客户端之后留3秒等待地图与Landscape实例进入Game世界。
        // 单客户端、未授权环境或普通Production启动不会自动推进测试天气。
        World->GetTimerManager().SetTimer(
            VillageSnowReviewTimer, this,
            &ADivineBeastsWorldGameMode::AdvanceVillageSnowReviewWeather,
            3.f, false);
        UE_LOG(LogTemp, Display,
            TEXT("VILLAGE_SNOW_REVIEW_TWO_CLIENTS_READY：3秒后开始权威天气四阶段测试。"));
    }
}
#endif

#if !UE_BUILD_SHIPPING
void ADivineBeastsWorldGameMode::AdvanceVillageSnowReviewWeather()
{
    if (bVillageGroundSnowReview)
    {
        AdvanceVillageGroundSnowReviewWeather();
        return;
    }
    UWorld* World = GetWorld();
    if (bWeatherShuttingDown || !HasAuthority() || !IsValid(World) ||
        World->bIsTearingDown || World->GetNetMode() != NM_DedicatedServer ||
        World->GetOutermost()->GetName() !=
            TEXT("/DBAWorldPack_Village/Maps/L_Village_Start"))
    {
        return;
    }
    // 审查步骤只在本次测试专服的有限生命期存在。
    // 生产天气状态机、随机调度及网络复制全部使用原GamePlatformWeather实现。
    if (VillageSnowReviewStep < 0 || VillageSnowReviewStep > 3) return;
    UGamePlatformWeatherWorldSubsystem* Weather =
        World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    if (!Weather) return;

    const int32 Step = VillageSnowReviewStep++;
    FGamePlatformWeatherState Target;
    switch (Step)
    {
    case 0: // 小雪：检测轻积雪可见且非零。
        Target.Type = EGamePlatformWeatherType::LightSnow;
        Target.SnowIntensity = .35f;
        Target.SnowAmount = .23f;
        Target.TemperatureCelsius = -2.f;
        break;
    case 1: // 大雪：检测两客户端雪量与特效意图。
    case 3: // 再次大雪：检测天气结束后可重新触发。
        Target.Type = EGamePlatformWeatherType::HeavySnow;
        Target.SnowIntensity = .90f;
        Target.SnowAmount = .85f;
        Target.WindIntensity = .4f;
        Target.TemperatureCelsius = -8.f;
        break;
    case 2: // 晴天：雪量归零，视觉层应取消降雪。
        Target.Type = EGamePlatformWeatherType::Clear;
        break;
    default:
        return;
    }
    const bool bApplied = Weather->SetWeather(Target, 1.5f);
    const int32 Revision = Weather->GetCurrentSnapshot().Revision;
    UE_LOG(LogTemp, Display,
        TEXT("VILLAGE_SNOW_REVIEW_SERVER_STEP step=%d type=%d snow=%.3f revision=%d accepted=%d"),
        Step, static_cast<int32>(Target.Type), Target.SnowAmount,
        Revision, bApplied ? 1 : 0);
    if (!bApplied)
    {
        UE_LOG(LogTemp, Error, TEXT("VILLAGE_SNOW_REVIEW_SERVER_REJECTED：权威天气子系统拒绝测试状态。"));
    }

    if (bApplied && VillageSnowReviewStep <= 3)
    {
        // 小雪阶段保持9秒；首次大雪保持26秒为断开/重连客户端留出冷启动时间；
        // 晴天保持12秒，避免测试工具错误地在B客户端进入前结束权威天气。
        const float NextDelaySeconds = Step == 1 ? 26.f : (Step == 2 ? 12.f : 9.f);
        World->GetTimerManager().SetTimer(
            VillageSnowReviewTimer, this,
            &ADivineBeastsWorldGameMode::AdvanceVillageSnowReviewWeather,
            NextDelaySeconds, false);
    }
}

void ADivineBeastsWorldGameMode::AdvanceVillageGroundSnowReviewWeather()
{
    UWorld* World = GetWorld();
    if (bWeatherShuttingDown || !bVillageGroundSnowReview || !HasAuthority() ||
        !IsValid(World) || World->bIsTearingDown ||
        World->GetNetMode() != NM_DedicatedServer ||
        World->GetOutermost()->GetName() != TEXT("/DBAWorldPack_Village/Maps/L_Village_Start") ||
        VillageSnowReviewStep < 0 || VillageSnowReviewStep > 4) return;
    UGamePlatformWeatherWorldSubsystem* Weather = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    if (!Weather) return;

    FGamePlatformWeatherState Target;
    float TransitionSeconds = 4.f;
    float HoldAndTransitionSeconds = 30.f;
    const int32 Step = VillageSnowReviewStep++;
    const TCHAR* Phase = TEXT("LightSnow");
    switch (Step)
    {
    case 0:
        Target.Type = EGamePlatformWeatherType::LightSnow;
        Target.SnowIntensity = .35f;
        Target.SnowAmount = .23f;
        Target.TemperatureCelsius = -2.f;
        break;
    case 1:
        Phase = TEXT("HeavySnow");
        Target.Type = EGamePlatformWeatherType::HeavySnow;
        Target.SnowIntensity = .90f;
        Target.SnowAmount = .85f;
        Target.WindIntensity = .25f;
        Target.TemperatureCelsius = -8.f;
        TransitionSeconds = 18.f;
        HoldAndTransitionSeconds = 60.f;
        break;
    case 2:
        // 停止降雪仅撤销雪花，低温下保留原积雪，不能同时切回绿色地面。
        Phase = TEXT("StoppedSnow");
        Target.Type = EGamePlatformWeatherType::Cloudy;
        Target.SnowAmount = .85f;
        Target.TemperatureCelsius = -4.f;
        TransitionSeconds = 2.f;
        HoldAndTransitionSeconds = 35.f;
        break;
    case 3:
        // 升温后沿服务器时间逐渐退雪并留下湿地；无逐粒子复制、无客户端自行改天气。
        Phase = TEXT("MeltingSnow");
        Target.Type = EGamePlatformWeatherType::Cloudy;
        Target.Wetness = .65f;
        Target.TemperatureCelsius = 6.f;
        TransitionSeconds = 40.f;
        HoldAndTransitionSeconds = 55.f; // 融雪完成后保留15秒无雪湿地，供双端取证。
        break;
    case 4:
        // 四阶段审核结束后恢复积雪并保持，避免人工查看时只能看到已融尽地面。
        Phase = TEXT("HeldHeavySnow");
        Target.Type = EGamePlatformWeatherType::HeavySnow;
        Target.SnowIntensity = .90f;
        Target.SnowAmount = .85f;
        Target.WindIntensity = .25f;
        Target.TemperatureCelsius = -8.f;
        TransitionSeconds = 10.f;
        break;
    default: return;
    }
    const bool bApplied = Weather->SetWeather(Target, TransitionSeconds);
    UE_LOG(LogTemp, Display,
        TEXT("VILLAGE_GROUND_SNOW_SERVER_PHASE step=%d phase=%s snow=%.3f intensity=%.3f wet=%.3f transition=%.1f revision=%d accepted=%d"),
        Step, Phase, Target.SnowAmount, Target.SnowIntensity, Target.Wetness, TransitionSeconds,
        Weather->GetCurrentSnapshot().Revision, bApplied ? 1 : 0);
    if (bApplied && VillageSnowReviewStep <= 4)
    {
        World->GetTimerManager().SetTimer(VillageSnowReviewTimer, this,
            &ADivineBeastsWorldGameMode::AdvanceVillageSnowReviewWeather,
            HoldAndTransitionSeconds, false);
    }
}
#endif

void ADivineBeastsWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bWeatherShuttingDown = true;
#if !UE_BUILD_SHIPPING
    bVillageSnowReviewArmed = false;
    bVillageSnowReviewStarted = false;
#endif
#if !UE_BUILD_SHIPPING
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(VillageSnowReviewTimer);
    }
#endif
    if (InitialWeatherPresetLease.IsValid())
    {
        UWorld* World = GetWorld();
        UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
        if (IGamePlatformDataService* Data =
                IsValid(Instance) ? IGamePlatformDataService::Get(*Instance) : nullptr)
        {
            Data->ReleaseDefinition(InitialWeatherPresetLease);
        }
        InitialWeatherPresetLease = {};
    }
    Super::EndPlay(EndPlayReason);
}

