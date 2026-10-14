// 纯客户端水面质量与几何门禁：只向GamePlatformPresentation提交中立VFX语义。
// 唯一Niagara播放和Data资产租约均归已存在的GamePlatformVFX/Presentation，不重建播放器。
#include "Subsystems/GamePlatformWaterClientWorldSubsystem.h"

#include "Definitions/GamePlatformWaterPresentationPreset.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Components/GamePlatformWaterInteractorComponent.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "GameplayTagsManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

#include "GameFramework/PlayerController.h"
// UE5.8相机管理器的公开头归Camera目录；保留并行水体表现逻辑，仅修正引擎包含路径。
#include "Camera/PlayerCameraManager.h"

#include "WaterBodyActor.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"



namespace
{
/** 只向当前世界本地玩家的统一表现服务发送语义；无有效目录或VFX提供者时失败关闭。
 * 此处永远不直接调用NiagaraFunctionLibrary或加载第三方流体插件。
 */
bool SubmitWaterVisualRequest(
    UWorld* World, FName SemanticName, const FVector& SurfacePoint, float Magnitude)
{
    if (!IsValid(World) || SemanticName.IsNone() || !World->GetGameInstance())
        return false;
    const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(SemanticName, false);
    if (!Tag.IsValid())
        return false;

    bool bAccepted = false;
    for (ULocalPlayer* Player : World->GetGameInstance()->GetLocalPlayers())
    {
        if (!IsValid(Player) || Player->GetWorld() != World)
            continue;
        UGamePlatformPresentationClientSubsystem* Presentation =
            Player->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
        if (!IsValid(Presentation))
            continue;
        FGamePlatformPresentationRequest Request;
        Request.RequestId = FGuid::NewGuid();
        Request.SemanticTag = Tag;
        Request.SourceId = TEXT("GamePlatformWater");
        Request.ContextId = TEXT("WorldWater");
        Request.SourceLocation = SurfacePoint;
        Request.ImpactLocation = SurfacePoint;
        Request.Magnitude = FMath::Clamp(Magnitude, 0.f, 10.f);
        Request.Priority = EGamePlatformPresentationPriority::Low;
        Request.Lifetime = EGamePlatformPresentationLifetime::Instant;
        Request.PredictionState = EGamePlatformPresentationPredictionState::Confirmed;
        // Submitted表示Provider已受理请求，不代表GPU完成渲染。资源出错不修改服务器水域状态。
        bAccepted |= Presentation->Submit(Request) ==
            EGamePlatformPresentationSubmitResult::Submitted;
    }
    return bAccepted;
}
}
void UGamePlatformWaterClientWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UWorld* World = GetWorld();
    if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
        return;
    // 使用引擎当前视觉特效档位初始化水面局部效果预算，避免低配机默认为高档。
    // 后续玩家修改画质由Settings插件显式调用SetWaterQualityTier刷新，不在Tick轮询。
    if (GEngine)
    {
        if (const UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
        {
            const int32 VisualTier = UserSettings->GetVisualEffectQuality();
            const EGamePlatformWaterQualityTier Selected = VisualTier <= 0
                ? EGamePlatformWaterQualityTier::Low
                : VisualTier == 1 ? EGamePlatformWaterQualityTier::Medium
                : VisualTier == 2 ? EGamePlatformWaterQualityTier::High
                : EGamePlatformWaterQualityTier::Cinematic;
            SetWaterQualityTier(Selected);
        }
    }
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

    // 用户自定义水体预设只包含语义和数值门禁，不直接预加载Niagara。
    // 真实Definition由上层内容包经GamePlatformData租约发布到Presentation目录。
    PresetLease = Preset;
    return !Preset->RippleSemanticTag.IsNone();
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

    const FVector SurfacePoint(WorldPosition.X, WorldPosition.Y, Sample.SurfaceZCm);
    const bool bRippleAccepted =
        SubmitWaterVisualRequest(World, Preset->RippleSemanticTag, SurfacePoint, Strength);
    if (!bRippleAccepted)
        return false; // 真实资源目录还未就绪，不增加已受理预算或虚报粒子播放。

    if (bSplash && Preset->bEnableSplash &&
        QualityTier != EGamePlatformWaterQualityTier::Low)
    {
        SubmitWaterVisualRequest(World, Preset->SplashSemanticTag,
            SurfacePoint, FMath::Min(Strength * 0.85f, 10.f));
    }
    RecentRippleTimes.Add(Now);
    LastRippleLocation = WorldPosition;
    LastRippleTime = Now;
    return true;
}
