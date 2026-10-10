// 天气客户端不轮询业务事实：OnRep通知启动有限定时插值；下一个快照/世界退出撤销旧特效请求。
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"

#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "GameplayTagsManager.h"

namespace
{
/** 一次天气快照只提交有变化的类型语义；VFX/SFX实际资源需由内容目录提供。 */
const TCHAR* WeatherTagSuffix(const EGamePlatformWeatherType Type)
{
    switch (Type)
    {
    case EGamePlatformWeatherType::Clear: return TEXT("Clear");
    case EGamePlatformWeatherType::Cloudy: return TEXT("Cloudy");
    case EGamePlatformWeatherType::LightRain:
    case EGamePlatformWeatherType::HeavyRain: return TEXT("Rain");
    case EGamePlatformWeatherType::LightSnow:
    case EGamePlatformWeatherType::HeavySnow: return TEXT("Snow");
    case EGamePlatformWeatherType::Fog: return TEXT("Fog");
    case EGamePlatformWeatherType::Wind: return TEXT("Wind");
    case EGamePlatformWeatherType::Thunderstorm: return TEXT("Thunderstorm");
    case EGamePlatformWeatherType::Blizzard: return TEXT("Blizzard");
    default: return TEXT("Clear");
    }
}
}

bool UGamePlatformWeatherClientWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) &&
        World->GetNetMode() != NM_DedicatedServer && !IsRunningCommandlet();
}

void UGamePlatformWeatherClientWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    ActiveSnapshot = FGamePlatformWeatherSnapshot();
    LastVisualState = FGamePlatformWeatherState();
}

void UGamePlatformWeatherClientWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (auto* Source = InWorld.GetSubsystem<UGamePlatformWeatherWorldSubsystem>())
    {
        SourceSnapshotHandle = Source->AddSnapshotHandler(
            FGamePlatformWeatherSnapshotChanged::FDelegate::CreateUObject(
                this, &UGamePlatformWeatherClientWorldSubsystem::OnSnapshotReceived));
        const FGamePlatformWeatherSnapshot Initial = Source->GetCurrentSnapshot();
        if (Initial.Revision > 0) OnSnapshotReceived(Initial);
    }
}

void UGamePlatformWeatherClientWorldSubsystem::Deinitialize()
{
    bClosing = true;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TransitionTimer);
        if (auto* Source = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>())
            Source->RemoveSnapshotHandler(SourceSnapshotHandle);
    }
    CancelPreviousPresentation();
    SourceSnapshotHandle.Reset();
    VisualChanged.Clear();
    Super::Deinitialize();
}

float UGamePlatformWeatherClientWorldSubsystem::GetEstimatedServerTimeSeconds() const
{
    const UWorld* World = GetWorld();
    if (!World) return 0.f;
    if (const AGameStateBase* GameState = World->GetGameState())
        return GameState->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds(); // 启动早期缺GameState时短暂回退；后续插值由真实同步时钟校正。
}

void UGamePlatformWeatherClientWorldSubsystem::OnSnapshotReceived(
    const FGamePlatformWeatherSnapshot& Snapshot)
{
    check(IsInGameThread());
    if (bClosing || Snapshot.Revision <= ActiveSnapshot.Revision) return;
    ActiveSnapshot = Snapshot;
    CancelPreviousPresentation();
    const FGamePlatformWeatherState Destination = Snapshot.To.Decode();
    SendPresentationEvent(Destination, false);
    UpdateTransition();
    if (UWorld* World = GetWorld())
    {
        const float Remaining = Snapshot.StartedAtServerSeconds + Snapshot.TransitionSeconds - GetEstimatedServerTimeSeconds();
        if (Remaining > 0.f && Snapshot.TransitionSeconds > 0.f)
            World->GetTimerManager().SetTimer(TransitionTimer, this,
                &UGamePlatformWeatherClientWorldSubsystem::UpdateTransition, 0.1f, true);
    }
}

void UGamePlatformWeatherClientWorldSubsystem::UpdateTransition()
{
    if (bClosing || ActiveSnapshot.Revision <= 0) return;
    const float Now = GetEstimatedServerTimeSeconds();
    ApplyVisualState(ActiveSnapshot.Sample(Now));
    if (Now >= ActiveSnapshot.StartedAtServerSeconds + ActiveSnapshot.TransitionSeconds)
    {
        if (UWorld* World = GetWorld())
            World->GetTimerManager().ClearTimer(TransitionTimer);
    }
}

void UGamePlatformWeatherClientWorldSubsystem::ApplyVisualState(const FGamePlatformWeatherState& State)
{
    LastVisualState = State;
    if (UWorld* World = GetWorld())
    {
        if (IGamePlatformSurfaceService* Surface = IGamePlatformSurfaceService::Get(*World))
        {
            // Surface的Moss/SnowHeight及其他静态场景配置继续由已有所有者维护；
            // 动态天气只写被明确归属的六个字段，保持完整快照写入语义。
            FGamePlatformSurfaceEnvironmentState Visual = Surface->GetEnvironmentState();
            Visual.GlobalWetness = State.Wetness;
            Visual.GlobalSnowAmount = State.SnowAmount;
            Visual.GlobalPuddleAmount = State.PuddleAmount;
            Visual.RainIntensity = State.RainIntensity;
            Visual.SnowIntensity = State.SnowIntensity;
            Visual.TemperatureCelsius = State.TemperatureCelsius;
            Surface->ApplyEnvironmentState(Visual);
        }
    }
    VisualChanged.Broadcast(LastVisualState);
}

FDelegateHandle UGamePlatformWeatherClientWorldSubsystem::AddVisualChangedHandler(
    FGamePlatformWeatherVisualChanged::FDelegate Handler)
{
    check(IsInGameThread());
    return !bClosing && Handler.IsBound() ? VisualChanged.Add(MoveTemp(Handler)) : FDelegateHandle();
}

void UGamePlatformWeatherClientWorldSubsystem::RemoveVisualChangedHandler(FDelegateHandle Handle)
{
    check(IsInGameThread());
    if (Handle.IsValid()) VisualChanged.Remove(Handle);
}

void UGamePlatformWeatherClientWorldSubsystem::SendPresentationEvent(
    const FGamePlatformWeatherState& State, const bool bCancellation)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetGameInstance()) return;
    if (!bCancellation)
    {
        ActiveRequestId = FGuid::NewGuid();
        const FString Base = FString(TEXT("Presentation.Weather.")) + WeatherTagSuffix(State.Type);
        ActiveVfxTag = FName(*(Base + TEXT(".VFX")));
        ActiveSfxTag = FName(*(Base + TEXT(".SFX")));
    }
    for (ULocalPlayer* LocalPlayer : World->GetGameInstance()->GetLocalPlayers())
    {
        if (!IsValid(LocalPlayer) || LocalPlayer->GetWorld() != World) continue;
        auto* Presentation = LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
        if (!Presentation) continue;
        for (const FName TagName : {ActiveVfxTag, ActiveSfxTag})
        {
            const FGameplayTag SemanticTag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);
            if (!SemanticTag.IsValid()) continue; // 配置无标签时静默降级，不私建Native Tag。
            FGamePlatformPresentationRequest Request;
            Request.RequestId = ActiveRequestId;
            Request.SemanticTag = SemanticTag;
            Request.SourceId = TEXT("GamePlatformWeather");
            Request.ContextId = TEXT("WorldWeather");
            Request.Magnitude = FMath::Max(FMath::Max(State.RainIntensity, State.SnowIntensity), State.WindIntensity);
            Request.Priority = EGamePlatformPresentationPriority::Low;
            Request.Lifetime = EGamePlatformPresentationLifetime::Persistent;
            Request.PredictionState = bCancellation
                ? EGamePlatformPresentationPredictionState::Cancelled
                : EGamePlatformPresentationPredictionState::Confirmed;
            Presentation->Submit(Request); // 无已注册Provider时返回ProviderMissing，不误认为效果已经播放。
        }
    }
}

void UGamePlatformWeatherClientWorldSubsystem::CancelPreviousPresentation()
{
    if (ActiveRequestId.IsValid())
    {
        SendPresentationEvent(LastVisualState, true);
        ActiveRequestId.Invalidate();
        ActiveVfxTag = NAME_None;
        ActiveSfxTag = NAME_None;
    }
}
