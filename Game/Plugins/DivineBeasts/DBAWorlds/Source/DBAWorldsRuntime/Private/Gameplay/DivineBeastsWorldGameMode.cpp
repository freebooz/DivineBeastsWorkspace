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

void ADivineBeastsWorldGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bWeatherShuttingDown = true;
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

