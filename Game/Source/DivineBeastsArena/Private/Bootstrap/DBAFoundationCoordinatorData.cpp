#include "Bootstrap/DBAFoundationCoordinator.h"
#include "DBAFoundationProbeDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Loading/GamePlatformAssetManager.h"
#include "Types/GamePlatformVersion.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

const TCHAR* UDBAFoundationCoordinator::SandboxPackage()
{
    return TEXT("/Game/Development/Foundation/Maps/L_FoundationSandbox");
}

FGamePlatformResult UDBAFoundationCoordinator::ValidateConfiguration() const
{
    check(IsInGameThread());
    if (UE_BUILD_SHIPPING || IsRunningCommandlet() || IsRunningDedicatedServer() ||
        !FParse::Param(FCommandLine::Get(), TEXT("FoundationStandalone")))
    {
        return FGamePlatformResult::Failure(TEXT("DevelopmentEntryDisabled"), TEXT("当前上下文禁止启动基础玩家流程"));
    }
    FGamePlatformId Id;
    FGamePlatformId RoundTrip;
    FGamePlatformVersion Version;
    if (!FGamePlatformId::TryParse(TEXT("Foundation.Probe@1"), Id) ||
        !FGamePlatformId::TryParse(Id.ToString(), RoundTrip) || Id != RoundTrip ||
        GetTypeHash(Id) != GetTypeHash(RoundTrip) ||
        !FGamePlatformVersion::TryParse(TEXT("0.1.0"), Version) || Version.ToString() != TEXT("0.1.0") ||
        FGamePlatformResult().IsSuccess())
    {
        return FGamePlatformResult::Failure(TEXT("CoreContractFailed"), TEXT("核心身份回环、哈希、版本或默认结果契约失败"));
    }
    UGameInstance* Instance = OwnerInstance.Get();
    if (!Instance || !IGamePlatformDataService::Get(*Instance) ||
        !Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized()))
    {
        return FGamePlatformResult::Failure(TEXT("DataServiceUnavailable"), TEXT("本实例数据门面或配置的资产管理器未就绪"));
    }
    if (!FPackageName::DoesPackageExist(SandboxPackage()))
    {
        return FGamePlatformResult::Failure(TEXT("SandboxMissing"), TEXT("真实基础测试地图尚未生成或未烘焙"));
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UDBAFoundationCoordinator::AdoptProbe(FGamePlatformDataLease& InOutLease)
{
    check(IsInGameThread());
    UGameInstance* Instance = OwnerInstance.Get();
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data || !Cast<UDBAFoundationProbeDefinition>(Data->GetLoadedDefinition(InOutLease)))
    {
        return FGamePlatformResult::Failure(TEXT("ProbeNotReady"), TEXT("只能转移属于本实例且已就绪的探针租约"));
    }
    if (ProbeLease.IsValid()) { Data->ReleaseDefinition(ProbeLease); }
    ProbeLease = InOutLease;
    InOutLease = {};
    return FGamePlatformResult::Success();
}

bool UDBAFoundationCoordinator::ReadProbe(int32& OutValue) const
{
    check(IsInGameThread());
    OutValue = 0;
    UGameInstance* Instance = OwnerInstance.Get();
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    const auto* Probe = Data ? Cast<UDBAFoundationProbeDefinition>(Data->GetLoadedDefinition(ProbeLease)) : nullptr;
    if (!Probe) { return false; }
    OutValue = Probe->ProbeValue;
    return true;
}

bool UDBAFoundationCoordinator::IsFoundationReady() const
{
    UGameInstance* Instance = OwnerInstance.Get();
    UWorld* World = Instance ? Instance->GetWorld() : nullptr;
    int32 ProbeValue = 0;
    if (!World || AcceptedSandboxWorld.Get() != World || AcceptedTravelOperation.IsEmpty() ||
        AcceptedTravelFlow.ScopeId != ActiveFlow.ScopeId || AcceptedTravelFlow.RunId != ActiveFlow.RunId ||
        FString(World->URL.GetOption(TEXT("FoundationTravel="), TEXT(""))) != AcceptedTravelOperation ||
        World->GetGameInstance() != Instance || !World->HasBegunPlay() ||
        UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != SandboxPackage() || !ReadProbe(ProbeValue)) { return false; }
    for (ULocalPlayer* Player : Instance->GetLocalPlayers())
    {
        const APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
        if (Controller && Controller->IsLocalController() && Controller->GetPawn()) { return true; }
    }
    return false;
}

bool UDBAFoundationCoordinator::AcceptSandboxWorld(UWorld& World, const FString& OperationId,
    const FGamePlatformFlowHandle& FlowHandle)
{
    check(IsInGameThread());
    auto* Instance = OwnerInstance.Get();
    if (bStopping || !Instance || !FlowHandle.IsValid() || FlowHandle.ScopeId != ActiveFlow.ScopeId ||
        FlowHandle.RunId != ActiveFlow.RunId || Instance->GetWorld() != &World ||
        World.GetGameInstance() != Instance || !World.HasBegunPlay() || OperationId.IsEmpty() ||
        UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()) != SandboxPackage() ||
        FString(World.URL.GetOption(TEXT("FoundationTravel="), TEXT(""))) != OperationId) { return false; }
    AcceptedSandboxWorld = &World;
    AcceptedTravelOperation = OperationId;
    AcceptedTravelFlow = FlowHandle;
    return true;
}
