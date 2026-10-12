// 分批生成无需逐鸟Actor；在一个有限定时器中申请真实Presentation鸟群群组并回收自有Token。
// 路线对象与世界身份由项目高级飞行配置重复核验；任何错误停止后保留可定位中文原因。
#include "Environment/DivineBeastsBirdFlockBatchComponent.h"

#include "DivineBeastsPresentationClientSubsystem.h"
#include "Components/SplineComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Environment/BirdFlock/GamePlatformBirdFlockSplineMath.h"

UDivineBeastsBirdFlockBatchComponent::UDivineBeastsBirdFlockBatchComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

TArray<FString> UDivineBeastsBirdFlockBatchComponent::GetAvailableSpecies() const
{
    return {TEXT("Seagull"), TEXT("Swallow"), TEXT("Eagle")};
}

void UDivineBeastsBirdFlockBatchComponent::BeginPlay()
{
    bEndingPlay = false;
    Super::BeginPlay();
    if (bAutoStart)
    {
        FString Error;
        if (!StartBatches(Error))
            UE_LOG(LogTemp, Warning, TEXT("[BirdFlockBatch] 自动启动被拒绝：%s"), *Error);
    }
}

void UDivineBeastsBirdFlockBatchComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    StopBatches();
    Super::EndPlay(Reason);
}

bool UDivineBeastsBirdFlockBatchComponent::StartBatches(FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (bEndingPlay || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || !IsValid(Owner))
    {
        OutError = TEXT("鸟群生成器仅能由有效的客户端游戏世界启动。");
        return false;
    }
    if (GroupPrefix.IsNone() || BirdsPerGroup < 0 || BirdsPerGroup > 512 ||
        MaxActiveGroups < 1 || MaxActiveGroups > 16 || MaxBatches < 0 || MaxBatches > 10000 ||
        !FMath::IsFinite(SpawnIntervalSeconds) || SpawnIntervalSeconds < .25f ||
        SpawnIntervalSeconds > 600.f || SpawnHalfExtentsCm.ContainsNaN() ||
        SpawnHalfExtentsCm.GetMin() < 0.f ||
        !FMath::IsFinite(CruiseSpeedCmPerSecond) || CruiseSpeedCmPerSecond < 10.f ||
        CruiseSpeedCmPerSecond > 5000.f ||
        !GetAvailableSpecies().Contains(SpeciesId.ToString()))
    {
        OutError = TEXT("鸟群生成配置非法：鸟种/数量/群数/间隔/范围/巡航速度越界。");
        return false;
    }
    FDivineBeastsBirdFlockFlightSettings Validated = Flight;
    if (Flight.bUseSpline)
    {
        AActor* Source = IsValid(RouteActor) ? RouteActor.Get() : Owner;
        Validated.Spline = IsValid(Source) ? Source->FindComponentByClass<USplineComponent>() : nullptr;
    }
    if (!Validated.ValidateForWorld(World, OutError)) return false;
    UGameInstance* GI = World->GetGameInstance();
    ULocalPlayer* Player = GI ? GI->GetFirstGamePlayer() : nullptr;
    auto* FoundService = Player ? Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>() : nullptr;
    if (!FoundService)
    {
        OutError = TEXT("本地玩家正式表现服务尚未就绪。");
        return false;
    }
    // 在所有校验通过后才替换旧批次；无效请求不得停止正在飞行的合法群。
    StopBatches();
    Flight.Spline = Validated.Spline;
    Service = FoundService;
    Random.Initialize(Flight.Seed);
    SpawnedBatches = 0;
    NextSpawnWorldSeconds = World->GetTimeSeconds();
    bActive = true;
    ScheduleStep();
    if (!bActive)
    {
        OutError = LastError;
        return false;
    }
    // 不开每鸟定时器，单个组级Timer在未到下次生成时只维护已过期群。
    const float Interval = FMath::Clamp(FMath::Min(SpawnIntervalSeconds, 0.5f), 0.25f, 1.0f);
    World->GetTimerManager().SetTimer(ScheduleHandle, this,
        &UDivineBeastsBirdFlockBatchComponent::ScheduleStep, Interval, true);
    return true;
}

void UDivineBeastsBirdFlockBatchComponent::ReleaseGroup(const FActiveGroup& Group)
{
    if (auto* Current = Service.Get())
        Current->StopAmbientBirdFlockForOwner(Group.GroupId, Group.RegistrationToken);
}

bool UDivineBeastsBirdFlockBatchComponent::SpawnOneGroup(FString& OutError)
{
    OutError.Reset();
    auto* Current = Service.Get();
    UWorld* World = GetWorld();
    if (!Current || !World || World->bIsTearingDown || !IsValid(GetOwner()))
    {
        OutError = TEXT("群落服务或世界已失效。");
        return false;
    }
    FDivineBeastsBirdFlockFlightSettings Settings = Flight;
    if (!Settings.ValidateForWorld(World, OutError)) return false;
    const int32 EffectiveCount = FGamePlatformBirdFlockSplineMath::ResolveVisibleCount(
        BirdsPerGroup, Settings.QualityFraction);
    if (EffectiveCount == INDEX_NONE)
    {
        OutError = TEXT("无效群组数量/画质比例。");
        return false;
    }
    if (EffectiveCount == 0) return true;
    const FVector Delta(Random.FRandRange(-SpawnHalfExtentsCm.X, SpawnHalfExtentsCm.X),
        Random.FRandRange(-SpawnHalfExtentsCm.Y, SpawnHalfExtentsCm.Y),
        Random.FRandRange(-SpawnHalfExtentsCm.Z, SpawnHalfExtentsCm.Z));
    const FVector Origin = GetOwner()->GetActorLocation() + Delta;
    const FName GroupId(*FString::Printf(TEXT("%s.%s.%d"),
        *GroupPrefix.ToString(), *GetPathName(), SpawnedBatches));
    const FGuid Token = FGuid::NewGuid();
    if (!Current->StartAmbientBirdFlockForOwnerAdvanced(GroupId, SpeciesId, EffectiveCount,
        Origin, this, Token, Settings, OutError)) return false;

    FActiveGroup Active;
    Active.GroupId = GroupId;
    Active.RegistrationToken = Token;
    if (Settings.FlightMode == EDivineBeastsBirdFlockFlightMode::SplineOneWay)
    {
        const USplineComponent* Route = Settings.Spline.Get();
        Active.ExpireAtWorldSeconds = World->GetTimeSeconds() +
            (Route ? static_cast<double>(Route->GetSplineLength()) / CruiseSpeedCmPerSecond : 0.0);
    }
    Groups.Add(MoveTemp(Active));
    return true;
}

void UDivineBeastsBirdFlockBatchComponent::ScheduleStep()
{
    if (!bActive || bEndingPlay || !Service.IsValid() || !IsValid(GetOwner()) ||
        !GetWorld() || GetWorld()->bIsTearingDown)
    {
        StopBatches();
        return;
    }
    UWorld* World = GetWorld();
    const double Now = World->GetTimeSeconds();
    // 单程样条到终点撤销原RequestId，不依靠Emitter重复Burst或销毁共享Data租约。
    for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
    {
        if (Groups[Index].ExpireAtWorldSeconds > 0.0 &&
            Now >= Groups[Index].ExpireAtWorldSeconds)
        {
            ReleaseGroup(Groups[Index]);
            Groups.RemoveAt(Index);
        }
    }
    if (MaxBatches > 0 && SpawnedBatches >= MaxBatches)
    {
        if (Groups.IsEmpty()) StopBatches();
        return;
    }
    if (Now < NextSpawnWorldSeconds) return;
    if (Groups.Num() >= MaxActiveGroups)
    {
        // 溢出时仅回收本组件最早的群组，不调用全局StopAll。
        ReleaseGroup(Groups[0]);
        Groups.RemoveAt(0);
    }
    FString Error;
    if (!SpawnOneGroup(Error))
    {
        LastError = Error;
        UE_LOG(LogTemp, Warning, TEXT("[BirdFlockBatch] 生成受理失败：%s"), *LastError);
        StopBatches();
        return;
    }
    ++SpawnedBatches;
    NextSpawnWorldSeconds = Now + SpawnIntervalSeconds;
}

void UDivineBeastsBirdFlockBatchComponent::StopBatches()
{
    check(IsInGameThread());
    bActive = false;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ScheduleHandle);
    const TArray<FActiveGroup> Previous = MoveTemp(Groups);
    Groups.Reset();
    for (const FActiveGroup& Group : Previous) ReleaseGroup(Group);
    Service.Reset();
    NextSpawnWorldSeconds = 0.0;
}
