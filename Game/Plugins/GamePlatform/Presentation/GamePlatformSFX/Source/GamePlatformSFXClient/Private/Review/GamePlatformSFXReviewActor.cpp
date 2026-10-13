// 平台SFX审核入口只负责一份自有请求与延迟定时器；真实音频/资源租约由现有服务维护。
#include "Review/GamePlatformSFXReviewActor.h"
#include "Interfaces/IGamePlatformSFXService.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Types/GamePlatformId.h"

AGamePlatformSFXReviewActor::AGamePlatformSFXReviewActor()
{
    bReplicates = false;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = false;
    ReviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SFXReviewRoot"));
    SetRootComponent(ReviewRoot);
}

bool AGamePlatformSFXReviewActor::IsSupportedReviewWorld(const UWorld* World)
{
    return IsValid(World) && !World->bIsTearingDown && World->GetNetMode() != NM_DedicatedServer &&
        (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE ||
         World->WorldType == EWorldType::GamePreview);
}

void AGamePlatformSFXReviewActor::BeginPlay()
{
    Super::BeginPlay();
    bClosing = false;
#if !UE_BUILD_SHIPPING
    if (bPlayOnBeginPlay && IsSupportedReviewWorld(GetWorld()) && FMath::IsFinite(StartDelaySeconds))
    {
        // 弱UObject定时回调不延长Actor存活；为Data/GameInstance初始化留下一个调度间隔。
        GetWorldTimerManager().SetTimer(StartTimer,
            FTimerDelegate::CreateWeakLambda(this, [this]() { PlayReview(); }),
            FMath::Clamp(StartDelaySeconds, 0.05f, 60.0f), false);
    }
#endif
}

void AGamePlatformSFXReviewActor::EndPlay(const EEndPlayReason::Type Reason)
{
    bClosing = true;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(StartTimer);
    StopReview();
    Super::EndPlay(Reason);
}

bool AGamePlatformSFXReviewActor::PlayReview()
{
    check(IsInGameThread());
#if UE_BUILD_SHIPPING
    LastResult = EGamePlatformSFXResultCode::UnsupportedEnvironment;
    return false;
#else
    if (bClosing || IsActorBeingDestroyed() || !IsSupportedReviewWorld(GetWorld()))
    {
        LastResult = EGamePlatformSFXResultCode::InvalidWorld;
        return false;
    }
    FGamePlatformId Parsed;
    if (!FGamePlatformId::TryParse(DefinitionId.ToString(), Parsed) ||
        !FMath::IsFinite(VolumeMultiplier) || VolumeMultiplier < 0.0f || VolumeMultiplier > 1.0f)
    {
        LastResult = EGamePlatformSFXResultCode::InvalidRequest;
        return false;
    }
    // 停止旧请求可能产生公开终态重入；若其回调销毁本Actor则不能开始新播放。
    StopReview();
    if (bClosing || IsActorBeingDestroyed() || !IsSupportedReviewWorld(GetWorld())) return false;
    auto* Service = IGamePlatformSFXService::Get(this);
    if (!Service)
    {
        LastResult = EGamePlatformSFXResultCode::DataUnavailable;
        return false;
    }
    const uint64 ExpectedEpoch = ++RequestEpoch;
    FGamePlatformSFXRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.DefinitionId = DefinitionId;
    Request.ContextId = TEXT("Review.LocalSFX");
    Request.Location = GetActorLocation();
    Request.Rotation = GetActorRotation();
    Request.AttachComponent = ReviewRoot;
    Request.VolumeMultiplier = VolumeMultiplier;
    ActiveRequestId = Request.RequestId;
    const FGamePlatformSFXResult Result = Service->Play(Request);
    // 不跨服务回调长期保存Service指针；Scope变化时重新取得同World服务取消自有新请求。
    if (bClosing || IsActorBeingDestroyed() || ExpectedEpoch != RequestEpoch ||
        ActiveRequestId != Request.RequestId)
    {
        if (auto* Current = IGamePlatformSFXService::Get(this)) Current->StopByRequestId(Request.RequestId, 0.0f);
        return false;
    }
    LastResult = Result.Code;
    if (!Result.IsAccepted())
    {
        ActiveRequestId.Invalidate();
        ActiveHandle = {};
        return false;
    }
    ActiveHandle = Result.Handle;
    return true;
#endif
}

void AGamePlatformSFXReviewActor::StopReview()
{
    check(IsInGameThread());
    // 即使尚未创建RequestId也先取消自己的延迟自动播放，避免停止后Timer又重新发声。
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(StartTimer);
    ++RequestEpoch;
    const FGuid PreviousRequest = ActiveRequestId;
    ActiveRequestId.Invalidate();
    ActiveHandle = {};
    if (!PreviousRequest.IsValid()) return;
    if (auto* Service = IGamePlatformSFXService::Get(this)) Service->StopByRequestId(PreviousRequest, 0.0f);
}

bool AGamePlatformSFXReviewActor::IsReviewActive() const
{
    if (bClosing || !ActiveRequestId.IsValid() || !ActiveHandle.IsValid()) return false;
    auto* Service = IGamePlatformSFXService::Get(this);
    return Service && Service->IsActive(ActiveHandle);
}

EGamePlatformSFXPlaybackState AGamePlatformSFXReviewActor::GetReviewPlaybackState() const
{
    if (!ActiveHandle.IsValid()) return EGamePlatformSFXPlaybackState::Invalid;
    if (auto* Service = IGamePlatformSFXService::Get(this)) return Service->GetPlaybackSnapshot(ActiveHandle).State;
    return EGamePlatformSFXPlaybackState::Invalid;
}

FGamePlatformSFXDiagnostics AGamePlatformSFXReviewActor::GetReviewDiagnostics() const
{
    if (auto* Service = IGamePlatformSFXService::Get(this)) return Service->GetDiagnostics();
    return {};
}
