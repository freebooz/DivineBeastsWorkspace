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
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "GameplayTagsManager.h"
#include "Atmosphere/GamePlatformWeatherAtmosphere.h"

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
    SnapshotApplyGeneration = 0;
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
    Atmosphere.Reset(); // 恢复光雾基线并移除仅本地持有的临时雾。
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
    if (bClosing || !IsValid(GetWorld()) || GetWorld()->bIsTearingDown) return;
    if (Snapshot.Revision > 0 && Snapshot.Revision <= ActiveSnapshot.Revision) return;
    const uint64 ApplyGeneration = ++SnapshotApplyGeneration;
    if (Snapshot.Revision == 0)
    {
        // 复制Actor退出时先撤销旧请求；重入的新Actor快照不能被旧清空操作覆盖。
        CancelPreviousPresentation();
        if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
            !IsValid(GetWorld()) || GetWorld()->bIsTearingDown) return;
        GetWorld()->GetTimerManager().ClearTimer(TransitionTimer);
        ActiveSnapshot = FGamePlatformWeatherSnapshot();
        ApplyVisualState(FGamePlatformWeatherState());
        return;
    }
    const int32 ReceivedRevision = Snapshot.Revision;
    ActiveSnapshot = Snapshot;
    CancelPreviousPresentation();
    if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
        ActiveSnapshot.Revision != ReceivedRevision || !IsValid(GetWorld()) ||
        GetWorld()->bIsTearingDown) return;
    // 首帧按照服务器实际时间采样；取消/表现同步回调可能递归送来更新的快照。
    SampleAndApplyTransition(false);
    if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
        ActiveSnapshot.Revision != ReceivedRevision) return;
    SendPresentationEvent(LastVisualState, false);
    if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
        ActiveSnapshot.Revision != ReceivedRevision || !IsValid(GetWorld()) ||
        GetWorld()->bIsTearingDown) return;
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
    SampleAndApplyTransition(true);
}

void UGamePlatformWeatherClientWorldSubsystem::SampleAndApplyTransition(
    const bool bRefreshPresentation)
{
    if (bClosing || ActiveSnapshot.Revision <= 0) return;
    const int32 Revision = ActiveSnapshot.Revision;
    const uint64 ApplyGeneration = SnapshotApplyGeneration;
    const float ServerNow = GetEstimatedServerTimeSeconds();
    const FGamePlatformWeatherState Sampled = ActiveSnapshot.Sample(ServerNow);
    const EGamePlatformWeatherType PreviousType = LastVisualState.Type;
    const bool bFinished = ServerNow >=
        ActiveSnapshot.StartedAtServerSeconds + ActiveSnapshot.TransitionSeconds;
    ApplyVisualState(Sampled);
    if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
        ActiveSnapshot.Revision != Revision) return; // 允许观察者同步切换天气重入。

    const float CurrentIntensity =
        FMath::Max(Sampled.RainIntensity, Sampled.SnowIntensity);
    // 一次过渡最多在“天气种类切换”和“最终强度稳定”两个节点重启对应粒子/声音。
    // 避免每0.1秒用Corrected不停Stop/Spawn Niagara，产生闪烁和资源池抖动。
    const bool bIntensityCorrection = bFinished && LastPresentedIntensity >= 0.f &&
        FMath::Abs(CurrentIntensity - LastPresentedIntensity) >= 0.10f;
    if (bRefreshPresentation && (PreviousType != Sampled.Type || bIntensityCorrection))
    {
        CancelPreviousPresentation();
        if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
            ActiveSnapshot.Revision != Revision) return;
        SendPresentationEvent(Sampled, false);
        if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
            ActiveSnapshot.Revision != Revision) return;
    }

    if (bFinished)
    {
        if (UWorld* World = GetWorld())
            World->GetTimerManager().ClearTimer(TransitionTimer);
    }
}

void UGamePlatformWeatherClientWorldSubsystem::RefreshPresentationAfterContentActivation(
    const bool bVisual)
{
    check(IsInGameThread());
    if (bClosing || ActiveSnapshot.Revision <= 0 || !IsValid(GetWorld()) ||
        GetWorld()->bIsTearingDown) return;

    // 内容包迟于OnRep加载时只刷新所属领域；不能因为SFX定义晚到
    // 就撤销、重建已经在播放的Niagara雨雪实例（反之亦然）。
    const int32 Channel = bVisual ? 0 : 1;
    const uint64 ApplyGeneration = SnapshotApplyGeneration;
    const FGamePlatformWeatherState Current =
        ActiveSnapshot.Sample(GetEstimatedServerTimeSeconds());
    CancelPreviousPresentation(Channel);
    if (bClosing || SnapshotApplyGeneration != ApplyGeneration ||
        !IsValid(GetWorld()) || GetWorld()->bIsTearingDown) return;
    SendPresentationEvent(Current, false, Channel);
}

void UGamePlatformWeatherClientWorldSubsystem::ApplyVisualState(const FGamePlatformWeatherState& State)
{
    LastVisualState = State;
    ApplyAtmosphere(State);
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
    // 派发稳定值快照，委托内部再次切换天气时不更改正在发出的数据。
    const FGamePlatformWeatherState PublishedState = LastVisualState;
    VisualChanged.Broadcast(PublishedState);
}

void UGamePlatformWeatherClientWorldSubsystem::ConfigureAtmosphere(const FGamePlatformWeatherAtmosphereProfile& Profile)
{
    check(IsInGameThread());
    Atmosphere.Reset();
    if (bClosing || !Profile.bEnabled || !IsValid(GetWorld()) || GetWorld()->bIsTearingDown ||
        GetWorld()->GetNetMode() == NM_DedicatedServer) return;
    Atmosphere = MakeShared<FGamePlatformWeatherAtmosphere>(*GetWorld(), Profile);
    ApplyAtmosphere(LastVisualState);
}

void UGamePlatformWeatherClientWorldSubsystem::ApplyAtmosphere(const FGamePlatformWeatherState& State)
{
    if (!Atmosphere) return;
    float Cloud = FGamePlatformWeatherAtmosphere::CloudWeight(State);
    if (ActiveSnapshot.Revision > 0)
    {
        const float Alpha = ActiveSnapshot.TransitionSeconds > 0.f ? FMath::Clamp(
            (GetEstimatedServerTimeSeconds() - ActiveSnapshot.StartedAtServerSeconds) /
            ActiveSnapshot.TransitionSeconds, 0.f, 1.f) : 1.f;
        Cloud = FMath::Lerp(FGamePlatformWeatherAtmosphere::CloudWeight(ActiveSnapshot.From.Decode()),
            FGamePlatformWeatherAtmosphere::CloudWeight(ActiveSnapshot.To.Decode()), Alpha);
    }
    Atmosphere->Apply(Cloud, FMath::Max(State.FogIntensity, State.SnowIntensity));
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
    const FGamePlatformWeatherState& State, const bool bCancellation,
    const int32 ChannelFilter, const FGuid CancellationRequestId,
    const FName CancellationTag)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetGameInstance() || (bClosing && !bCancellation)) return;
    const uint64 DispatchGeneration = SnapshotApplyGeneration;
    // P0只有雨与雪对应可加载的持续VFX/SFX目录。晴、阴、风、雾等仅由
    // Surface/未来Sky适配处理；此处不提交尚无Definition的虚假表现请求。
    // 取消请求必须保留原请求ID，不受此过滤影响。
    const bool bRain = State.Type == EGamePlatformWeatherType::LightRain ||
        State.Type == EGamePlatformWeatherType::HeavyRain;
    const bool bSnow = State.Type == EGamePlatformWeatherType::LightSnow ||
        State.Type == EGamePlatformWeatherType::HeavySnow;
    if (!bCancellation && !bRain && !bSnow) return;
    if (!bCancellation)
    {
        LastPresentedIntensity = bRain || bSnow
            ? FMath::Max(State.RainIntensity, State.SnowIntensity) : -1.f;
    }
    if (!bCancellation)
    {
        const FString Base = FString(TEXT("Presentation.Weather.")) +
            WeatherTagSuffix(State.Type);
        if (ChannelFilter < 0 || ChannelFilter == 0)
        {
            ActiveVfxRequestId = FGuid::NewGuid();
            ActiveVfxTag = FName(*(Base + TEXT(".VFX")));
        }
        if (ChannelFilter < 0 || ChannelFilter == 1)
        {
            ActiveSfxRequestId = FGuid::NewGuid();
            ActiveSfxTag = FName(*(Base + TEXT(".SFX")));
        }
    }
    // 保留本次待提交身份：同步Provider回调可能发起新快照，不允许旧提交读取新请求ID。
    const FGuid SubmissionVfxId = ActiveVfxRequestId;
    const FGuid SubmissionSfxId = ActiveSfxRequestId;
    const FName SubmissionVfxTag = ActiveVfxTag;
    const FName SubmissionSfxTag = ActiveSfxTag;
    for (ULocalPlayer* LocalPlayer : World->GetGameInstance()->GetLocalPlayers())
    {
        if (!bCancellation && (bClosing || SnapshotApplyGeneration != DispatchGeneration)) return;
        if (!IsValid(LocalPlayer) || LocalPlayer->GetWorld() != World) continue;
        auto* Presentation = LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
        if (!Presentation) continue;
        for (int32 Channel = 0; Channel < 2; ++Channel)
        {
            if (ChannelFilter >= 0 && ChannelFilter != Channel) continue;
            const FGuid ChannelId = bCancellation ? CancellationRequestId :
                (Channel == 0 ? SubmissionVfxId : SubmissionSfxId);
            if (!ChannelId.IsValid()) continue;
            const FName TagName = bCancellation ? CancellationTag :
                (Channel == 0 ? SubmissionVfxTag : SubmissionSfxTag);
            if (!bCancellation && (bClosing || SnapshotApplyGeneration != DispatchGeneration ||
                (Channel == 0 && ActiveVfxRequestId != ChannelId) ||
                (Channel == 1 && ActiveSfxRequestId != ChannelId))) return;
            const FGameplayTag SemanticTag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);
            if (!SemanticTag.IsValid()) continue; // 配置无标签时静默降级，不私建Native Tag。
            FGamePlatformPresentationRequest Request;
            Request.RequestId = ChannelId;
            Request.SemanticTag = SemanticTag;
            Request.SourceId = TEXT("GamePlatformWeather");
            Request.ContextId = TEXT("WorldWeather");
            // 天气视觉必须跟随本地观察主体，不能永远在世界原点生成雨雪。
            // Attached VFX Definition按玩家ViewTarget根组件持续跟随；不持有强指针。
            if (APlayerController* Controller = LocalPlayer->GetPlayerController(World))
            {
                FVector ViewPosition = FVector::ZeroVector;
                FRotator ViewRotation = FRotator::ZeroRotator;
                Controller->GetPlayerViewPoint(ViewPosition, ViewRotation);
                Request.SourceLocation = ViewPosition;
                if (AActor* Target = Controller->GetViewTarget())
                {
                    Request.AttachComponent = Target->GetRootComponent();
                }
            }
            Request.Magnitude = FMath::Max(FMath::Max(State.RainIntensity, State.SnowIntensity), State.WindIntensity);
            // Rain/Snow两档共用一套资源；客户端按Definition白名单传递实际0..1强度。
            // VFX以Niagara原生User参数接收，SFX以MetaSound允许的浮点参数接收，
            // 资源不支持此参数时由领域Definition拒绝，不偷偷播放错误强度的天气。
            const bool bParametricWeather = bRain || bSnow;
            if (bParametricWeather)
            {
                const float Strength = FMath::Clamp(
                    FMath::Max(State.RainIntensity, State.SnowIntensity), 0.f, 1.f);
                Request.FloatParameters.Add(
                    Channel == 0 ? FName(TEXT("User.WeatherIntensity")) :
                        FName(TEXT("WeatherIntensity")), Strength);
                if (Channel == 0)
                {
                    // Niagara两级雨雪Emitter分别用原生SpawnRate绑定，强度真正改变发射率，
                    // 不是仅透传一个未绑定的WeatherIntensity数值。单位：粒子/秒。
                    Request.FloatParameters.Add(TEXT("User.WeatherNearSpawnRate"),
                        (bRain ? 1400.f : 500.f) * Strength);
                    Request.FloatParameters.Add(TEXT("User.WeatherFarSpawnRate"),
                        (bRain ? 800.f : 320.f) * Strength);
                }
                // SoundWave尚未接上动态MetaSound时仍能按强度控制请求级音量。
                // Niagara一侧保留默认1倍，防止天气光效被声学系数污染。
                if (Channel == 1)
                    Request.VolumeMultiplier = FMath::Lerp(.15f, 1.f, Strength);
            }
            Request.Priority = EGamePlatformPresentationPriority::Low;
            Request.Lifetime = EGamePlatformPresentationLifetime::Persistent;
            Request.PredictionState = bCancellation
                ? EGamePlatformPresentationPredictionState::Cancelled
                : EGamePlatformPresentationPredictionState::Confirmed;
            Presentation->Submit(Request); // 无已注册Provider时返回ProviderMissing，不误认为效果已经播放。
        }
    }
}

void UGamePlatformWeatherClientWorldSubsystem::CancelPreviousPresentation(
    const int32 ChannelFilter)
{
    const bool bCancelVfx = ChannelFilter < 0 || ChannelFilter == 0;
    const bool bCancelSfx = ChannelFilter < 0 || ChannelFilter == 1;
    const FGuid OldVfxId = ActiveVfxRequestId;
    const FGuid OldSfxId = ActiveSfxRequestId;
    const FName OldVfxTag = ActiveVfxTag;
    const FName OldSfxTag = ActiveSfxTag;
    const FGamePlatformWeatherState OldState = LastVisualState;
    // 先取走旧身份并失效，再同步提交取消。重入创建的新请求绝不可由旧取消栈清空。
    if (bCancelVfx) { ActiveVfxRequestId.Invalidate(); ActiveVfxTag = NAME_None; }
    if (bCancelSfx) { ActiveSfxRequestId.Invalidate(); ActiveSfxTag = NAME_None; }
    if (!ActiveVfxRequestId.IsValid() && !ActiveSfxRequestId.IsValid())
        LastPresentedIntensity = -1.f;
    if (bCancelVfx && OldVfxId.IsValid())
        SendPresentationEvent(OldState, true, 0, OldVfxId, OldVfxTag);
    if (bCancelSfx && OldSfxId.IsValid())
        SendPresentationEvent(OldState, true, 1, OldSfxId, OldSfxTag);
}
