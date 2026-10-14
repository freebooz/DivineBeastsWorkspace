// 纯客户端Water+Niagara表现执行：预设异步加载、时间预算与可见距离节流。
// 只有本世界水域已确认才生成粒子，不进行GPU流体求解或跨客户端状态同步。
#include "Subsystems/GamePlatformWaterClientWorldSubsystem.h"

#include "Definitions/GamePlatformWaterPresentationPreset.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Components/GamePlatformWaterInteractorComponent.h"
#include "Loading/GamePlatformAssetLoader.h"

#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "WaterBodyActor.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"


void UGamePlatformWaterClientWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UWorld* World = GetWorld();
    if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
        return;
    // 不轮询世界Actor：只订阅真实Spawn事件，并一次性接线已存在的角色Pawn。
    ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this,
            &UGamePlatformWaterClientWorldSubsystem::ObserveSpawnedActor));
    for (TActorIterator<APawn> It(World); It; ++It)
        ObserveSpawnedActor(*It);
}

void UGamePlatformWaterClientWorldSubsystem::ObserveSpawnedActor(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->GetWorld() != GetWorld() ||
        !Cast<APawn>(Actor))
        return;
    UGamePlatformWaterInteractorComponent* WaterInteractor =
        Actor->FindComponentByClass<UGamePlatformWaterInteractorComponent>();
    if (!IsValid(WaterInteractor) || WaterPresenceBindings.Contains(WaterInteractor))
        return;
    const FDelegateHandle Handle = WaterInteractor->OnWaterStateChangedNative.AddUObject(
        this, &UGamePlatformWaterClientWorldSubsystem::HandleWaterPresenceChanged);
    WaterPresenceBindings.Add(WaterInteractor, Handle);
}

void UGamePlatformWaterClientWorldSubsystem::HandleWaterPresenceChanged(
    const AActor* Actor, const FGamePlatformWaterPresence& State)
{
    if (!State.bInWater || !IsValid(Actor) || Actor->GetWorld() != GetWorld())
        return;
    const APawn* Pawn = Cast<APawn>(Actor);
    if (!IsValid(Pawn) || !Pawn->IsLocallyControlled())
        return;
    // 水域刚进入时产生一次本地涟漪与轻量飞溅。脚步AnimNotify可独立调用本服务，
    // 但它不改变这里的服务器权威Presence，也不创建新的网络状态。
    const FVector FootContact = Pawn->GetActorLocation() + FVector(0.,0.,-85.);
    RequestLocalRipple(FootContact, State.bDeepWater ? 1.6f : 0.7f, true);
}

void UGamePlatformWaterClientWorldSubsystem::UnbindWaterPresence()
{
    if (UWorld* World = GetWorld())
        if (ActorSpawnedHandle.IsValid())
            World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    ActorSpawnedHandle.Reset();
    for (const auto& Pair : WaterPresenceBindings)
        if (UGamePlatformWaterInteractorComponent* Component = Pair.Key.Get())
            Component->OnWaterStateChangedNative.Remove(Pair.Value);
    WaterPresenceBindings.Reset();
}
bool UGamePlatformWaterClientWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld() &&
        World->GetNetMode() != NM_DedicatedServer &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformWaterClientWorldSubsystem::Deinitialize()
{
    UnbindWaterPresence();
    ClearPresentation();
    Super::Deinitialize();
}

void UGamePlatformWaterClientWorldSubsystem::ClearPresentation()
{
    ++Generation;
    if (PendingLoad.IsValid())
        FGamePlatformAssetLoader::Cancel(PendingLoad);
    PendingLoad.Reset();
    PresetLease.Reset();
    RecentRippleTimes.Reset();
    LastRippleTime = -100.;
}

bool UGamePlatformWaterClientWorldSubsystem::SetWaterQualityTier(
    EGamePlatformWaterQualityTier NewTier)
{
    switch (NewTier)
    {
    case EGamePlatformWaterQualityTier::Low:
    case EGamePlatformWaterQualityTier::Medium:
    case EGamePlatformWaterQualityTier::High:
    case EGamePlatformWaterQualityTier::Cinematic:
        QualityTier = NewTier;
        RecentRippleTimes.Reset(); // 档位切换不继承原档位的频率预算。
        return true;
    default:
        return false;
    }
}

bool UGamePlatformWaterClientWorldSubsystem::ConfigurePresentation(
    UGamePlatformWaterPresentationPreset* Preset)
{
    ClearPresentation();
    if (!IsValid(Preset) || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
        return false;
    if (Preset->MaxRippleSpawnsPerSecond < 0 || Preset->MaxRippleSpawnsPerSecond > 96 ||
        !FMath::IsFinite(Preset->MaxViewDistanceCm) ||
        Preset->MaxViewDistanceCm < 100.f || Preset->MaxViewDistanceCm > 30000.f ||
        !FMath::IsFinite(Preset->MinimalTimeBetweenSamePointSeconds) ||
        Preset->MinimalTimeBetweenSamePointSeconds < 0.01f ||
        Preset->MinimalTimeBetweenSamePointSeconds > 1.f)
        return false;

    PresetLease = Preset;
    TArray<FSoftObjectPath> Paths;
    if (!Preset->RippleSystem.IsNull()) Paths.Add(Preset->RippleSystem.ToSoftObjectPath());
    if (Preset->bEnableSplash && !Preset->SplashSystem.IsNull())
        Paths.AddUnique(Preset->SplashSystem.ToSoftObjectPath());
    if (Paths.IsEmpty())
        return true; // 缺资源仅代表已配置无粒子的轻量表现模式。

    const uint64 Captured = Generation;
    const TWeakObjectPtr<UGamePlatformWaterClientWorldSubsystem> WeakThis(this);
    PendingLoad = FGamePlatformAssetLoader::RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateLambda([WeakThis, Captured]()
        {
            UGamePlatformWaterClientWorldSubsystem* Self = WeakThis.Get();
            if (!IsValid(Self) || Captured != Self->Generation ||
                !IsValid(Self->GetWorld()))
                return;
            // SoftObjectPtr只读当前本地资源；无需重复申请或同步Load。
        }));
    return PendingLoad.IsValid();
}

int32 UGamePlatformWaterClientWorldSubsystem::GetNativeWaterBodyCount() const
{
    int32 Count = 0;
    const UWorld* World = GetWorld();
    if (World)
    {
        for (TActorIterator<AWaterBody> It(const_cast<UWorld*>(World)); It; ++It)
            if (IsValid(*It)) ++Count;
    }
    return Count;
}

bool UGamePlatformWaterClientWorldSubsystem::RequestLocalRipple(
    const FVector& WorldPosition, float Strength, bool bSplash)
{
    UWorld* World = GetWorld();
    const UGamePlatformWaterPresentationPreset* Preset = PresetLease.Get();
    if (!World || World->GetNetMode() == NM_DedicatedServer || !IsValid(Preset) ||
        !FMath::IsFinite(Strength) || Strength < 0.f || Strength > 10.f ||
        !FMath::IsFinite(WorldPosition.X) || !FMath::IsFinite(WorldPosition.Y) ||
        !FMath::IsFinite(WorldPosition.Z) ||
        Preset->MaxRippleSpawnsPerSecond == 0)
        return false;

    // 先按档位收紧请求预算，再进入任何UObject资源访问；无轮询和任何GPU模拟。
    int32 TierMaxPerSecond = 18;
    float TierMaxDistanceCm = 6000.f;
    switch (QualityTier)
    {
    case EGamePlatformWaterQualityTier::Low:
        TierMaxPerSecond = 3; TierMaxDistanceCm = 2000.f; break;
    case EGamePlatformWaterQualityTier::Medium:
        TierMaxPerSecond = 8; TierMaxDistanceCm = 4000.f; break;
    case EGamePlatformWaterQualityTier::High:
        TierMaxPerSecond = 18; TierMaxDistanceCm = 6000.f; break;
    case EGamePlatformWaterQualityTier::Cinematic:
        TierMaxPerSecond = 32; TierMaxDistanceCm = 12000.f; break;
    }
    const int32 EffectiveMaxSpawns = FMath::Min(Preset->MaxRippleSpawnsPerSecond, TierMaxPerSecond);
    const float EffectiveMaxViewDistanceCm = FMath::Min(Preset->MaxViewDistanceCm, TierMaxDistanceCm);
    if (EffectiveMaxSpawns <= 0) return false;
    const UGamePlatformWaterWorldSubsystem* Geometry = World->GetSubsystem<UGamePlatformWaterWorldSubsystem>();
    FGamePlatformWaterSample Sample;
    if (!Geometry || !Geometry->QueryWaterAtLocation(WorldPosition, Sample) ||
        !Sample.bAllowLocalRipples)
        return false;

    const APlayerController* Controller = World->GetFirstPlayerController();
    const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager : nullptr;
    if (!IsValid(Camera) || FVector::DistSquared(Camera->GetCameraLocation(), WorldPosition) >
        FMath::Square(EffectiveMaxViewDistanceCm))
        return false;

    const double Now = World->GetTimeSeconds();
    if (Now - LastRippleTime < Preset->MinimalTimeBetweenSamePointSeconds &&
        FVector::DistSquared2D(WorldPosition, LastRippleLocation) < FMath::Square(65.f))
        return false;
    RecentRippleTimes.RemoveAll([Now](double Time) { return Now - Time >= 1.0; });
    if (RecentRippleTimes.Num() >= EffectiveMaxSpawns)
        return false;

    UNiagaraSystem* Ripple = Preset->RippleSystem.Get();
    if (!IsValid(Ripple))
        return false; // 资源未加载不阻止Gameplay，也绝不同步加载。
    UNiagaraComponent* RippleComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World, Ripple, FVector(WorldPosition.X, WorldPosition.Y, Sample.SurfaceZCm),
        FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
    if (!IsValid(RippleComponent))
        return false;

    RippleComponent->SetVariableFloat(TEXT("User.RippleStrength"), Strength);
    if (bSplash && Preset->bEnableSplash &&
        QualityTier != EGamePlatformWaterQualityTier::Low)
    {
        if (UNiagaraSystem* Splash = Preset->SplashSystem.Get())
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                World, Splash, FVector(WorldPosition.X, WorldPosition.Y, Sample.SurfaceZCm),
                FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
        }
    }
    RecentRippleTimes.Add(Now);
    LastRippleLocation = WorldPosition;
    LastRippleTime = Now;
    return true;
}
