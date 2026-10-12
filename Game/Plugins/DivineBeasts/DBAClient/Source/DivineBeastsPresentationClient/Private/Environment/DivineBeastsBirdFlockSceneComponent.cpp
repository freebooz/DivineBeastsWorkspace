// 神兽联盟客户端场景组件：Begin/EndPlay事件建立/撤销自有注册，不启动独立渲染器。
// 身份替换先验证候选再撤销旧绑定；同步完成通知可重入，必须核对本组件变更token。
#include "Environment/DivineBeastsBirdFlockSceneComponent.h"
#include "Environment/BirdFlock/GamePlatformBirdFlockSplineMath.h"
#include "Components/SplineComponent.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

UDivineBeastsBirdFlockSceneComponent::UDivineBeastsBirdFlockSceneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

FName UDivineBeastsBirdFlockSceneComponent::GetConfiguredFlockId() const
{
    return FlockId.IsNone() ? FName(*GetPathName()) : FlockId;
}

TArray<FString> UDivineBeastsBirdFlockSceneComponent::GetBirdSpeciesOptions() const
{
    return {TEXT("Seagull"), TEXT("Swallow"), TEXT("Eagle")};
}

void UDivineBeastsBirdFlockSceneComponent::BeginPlay()
{
    bEndingPlay = false;
    Super::BeginPlay();
    FString Error;
    if (!ApplyBirdFlock(Error) && !Error.IsEmpty())
        UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 场景锚点：%s"), *Error);
}

bool UDivineBeastsBirdFlockSceneComponent::ApplyBirdFlock(FString& OutError)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsBirdFlockSceneComponent> KeepComponent(this);
    OutError.Reset();
    UWorld* World = GetWorld();
    if (bEndingPlay || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) return false;
    UGameInstance* GI = World->GetGameInstance();
    ULocalPlayer* Player = GI ? GI->GetFirstGamePlayer() : nullptr;
    auto* Service = Player ? Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>() : nullptr;
    if (!Service)
    {
        OutError = TEXT("本地玩家表现服务尚未建立。");
        return false;
    }

    const FGuid Operation = FGuid::NewGuid();
    ApplyMutationToken = Operation;
    const auto OldService = BoundPresentation;
    const FName OldId = BoundFlockId;
    const FGuid OldToken = BoundRegistrationToken;
    const FName NewId = GetConfiguredFlockId();
    const FGuid NewToken = OldService.Get() == Service && OldId == NewId && OldToken.IsValid()
        ? OldToken : FGuid::NewGuid();

    // 0只鸟的语义始终是撤销本Owner持有的请求，不得因为旧Spline已被关卡卸载而无法停播。
    if (BirdCount == 0)
    {
        StopBirdFlock();
        return true;
    }

    int32 DesiredCount = BirdCount;
    FDivineBeastsBirdFlockFlightSettings Flight = AdvancedFlight;
    if (bEnableAdvancedFlight)
    {
        // 优先在项目层查明路径Actor的所属世界，不把Actor指针放到跨插件公开数值协议。
        if (Flight.bUseSpline)
        {
            AActor* SourceActor = IsValid(RouteActor) ? RouteActor.Get() : GetOwner();
            Flight.Spline = IsValid(SourceActor) ? SourceActor->FindComponentByClass<USplineComponent>() : nullptr;
        }
        if (!Flight.ValidateForWorld(World, OutError)) return false;
        DesiredCount = FGamePlatformBirdFlockSplineMath::ResolveVisibleCount(
            BirdCount, Flight.QualityFraction);
        if (DesiredCount == INDEX_NONE)
        {
            OutError = TEXT("鸟群质量比例非法；旧播放未更改。");
            return false;
        }
    }
    // 不先Stop旧绑定：非法参数、跨世界Spline或重复身份不能污染合法旧请求。
    const bool bAccepted = bEnableAdvancedFlight
        ? Service->StartAmbientBirdFlockForOwnerAdvanced(NewId, SpeciesId, DesiredCount,
            GetComponentLocation(), this, NewToken, Flight, OutError)
        : Service->StartAmbientBirdFlockForOwner(NewId, SpeciesId, DesiredCount,
            GetComponentLocation(), this, NewToken, OutError);
    if (!bAccepted) return false;
    if (ApplyMutationToken != Operation || bEndingPlay)
    {
        // 重入操作已接管绑定。只清理无人认领的本次候选，不能撤销内层已经接纳的同一注册。
        if (BoundPresentation.Get() != Service || BoundFlockId != NewId || BoundRegistrationToken != NewToken)
            Service->StopAmbientBirdFlockForOwner(NewId, NewToken);
        OutError = TEXT("场景鸟群配置已在同步通知中被更新或停止。");
        return false;
    }

    if (DesiredCount > 0)
    {
        BoundPresentation = Service;
        BoundFlockId = NewId;
        BoundRegistrationToken = NewToken;
    }
    else
    {
        BoundPresentation.Reset(); BoundFlockId = NAME_None; BoundRegistrationToken.Invalidate();
    }
    // 先发布新绑定再取消旧句柄；旧取消通知若重入Apply，将看到最新注册身份。
    if (auto* Previous = OldService.Get();
        Previous && OldToken.IsValid() && (Previous != Service || OldId != NewId || OldToken != NewToken || DesiredCount == 0))
        Previous->StopAmbientBirdFlockForOwner(OldId, OldToken);
    if (ApplyMutationToken != Operation)
    {
        OutError = TEXT("场景鸟群绑定已被后续同步操作接管。");
        return false;
    }
    return true;
}

void UDivineBeastsBirdFlockSceneComponent::StopBirdFlock()
{
    check(IsInGameThread());
    ApplyMutationToken = FGuid::NewGuid();
    const auto Previous = BoundPresentation;
    const FName PreviousId = BoundFlockId;
    const FGuid PreviousToken = BoundRegistrationToken;
    BoundPresentation.Reset(); BoundFlockId = NAME_None; BoundRegistrationToken.Invalidate();
    if (auto* Service = Previous.Get()) Service->StopAmbientBirdFlockForOwner(PreviousId, PreviousToken);
}

void UDivineBeastsBirdFlockSceneComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    StopBirdFlock();
    Super::EndPlay(Reason);
}

#if WITH_EDITOR
void UDivineBeastsBirdFlockSceneComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if (GetWorld() && GetWorld()->IsGameWorld() && HasBegunPlay() && !bEndingPlay)
    {
        FString Error;
        ApplyBirdFlock(Error);
        if (!Error.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 参数修改被拒绝：%s"), *Error);
    }
}
#endif
