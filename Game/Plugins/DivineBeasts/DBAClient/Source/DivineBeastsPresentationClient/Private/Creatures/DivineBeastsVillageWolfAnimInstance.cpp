// 狼动画实例拥有本Mesh的事件订阅和Montage；不发送RPC，不写Combat，不持有服务器控制器。
#include "Creatures/DivineBeastsVillageWolfAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Characters/DivineBeastsVillageVampire.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void UDivineBeastsVillageWolfAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    ReleaseBindings();
    bClosing = false;
    bWolfDead = false;
    WolfSpeed = 0.0f;
    auto* Wolf = Cast<ADivineBeastsVillageWolf>(TryGetPawnOwner());
    UWorld* World = GetWorld();
    if (!IsValid(Wolf) || !World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer)
        return;
    ObservedWolf = Wolf;
    LifeSubscription = Wolf->OnWolfLifeChanged().AddUObject(this, &ThisClass::HandleLifeChanged);
    auto* CastComponent = Wolf->FindComponentByClass<UGamePlatformAbilityCastComponent>();
    ObservedCast = CastComponent;
    if (CastComponent)
        CastSubscription = CastComponent->OnCastFactsChanged().AddUObject(this, &ThisClass::HandleCastChanged);
    // 先绑定再读取一次快照；死亡和攻击的复制顺序不改变死亡优先约束。
    HandleLifeChanged(Wolf);
    if (CastComponent) HandleCastChanged(CastComponent, CastComponent->GetCastFacts());
}

void UDivineBeastsVillageWolfAnimInstance::NativeUninitializeAnimation()
{
    ReleaseBindings();
    Super::NativeUninitializeAnimation();
}

void UDivineBeastsVillageWolfAnimInstance::ReleaseBindings()
{
    bClosing = true;
    if (auto* Wolf = ObservedWolf.Get()) Wolf->OnWolfLifeChanged().Remove(LifeSubscription);
    if (auto* Cast = ObservedCast.Get()) Cast->OnCastFactsChanged().Remove(CastSubscription);
    LifeSubscription.Reset();
    CastSubscription.Reset();
    ObservedWolf.Reset();
    ObservedCast.Reset();
    ConsumedBite.Invalidate();
    PlayingBite.Invalidate();
    if (BiteMontage) Montage_Stop(0.0f, BiteMontage);
    if (GreatSwordMontage) Montage_Stop(0.0f, GreatSwordMontage);
}

void UDivineBeastsVillageWolfAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const auto* Wolf = ObservedWolf.Get();
    // 引擎动画更新只采样已复制移动的连续速度供BlendSpace插值；业务状态完全由通知更新。
    WolfSpeed = !bClosing && !bWolfDead && IsValid(Wolf) && !Wolf->IsActorBeingDestroyed()
        ? GroundSpeedCmPerSecond : 0.0f;
}

void UDivineBeastsVillageWolfAnimInstance::HandleLifeChanged(ADivineBeastsVillageWolf* Wolf)
{
    if (bClosing || !IsValid(Wolf) || ObservedWolf.Get() != Wolf || Wolf->GetWorld() != GetWorld() ||
        Wolf->IsActorBeingDestroyed()) return;
    bWolfDead = Wolf->IsWolfDead();
    if (bWolfDead)
    {
        WolfSpeed = 0.0f;
        PlayingBite.Invalidate();
        if (BiteMontage) Montage_Stop(0.05f, BiteMontage);
        if (GreatSwordMontage) Montage_Stop(0.05f, GreatSwordMontage);
    }
}

void UDivineBeastsVillageWolfAnimInstance::HandleCastChanged(
    UGamePlatformAbilityCastComponent* Component, const FGamePlatformAbilityCastFacts& Incoming)
{
    auto* Wolf = ObservedWolf.Get();
    UWorld* World = GetWorld();
    if (bClosing || !World || World->bIsTearingDown || !IsValid(Wolf) || Wolf->IsActorBeingDestroyed() ||
        !IsValid(Component) || Component != ObservedCast.Get() || Component->GetOwner() != Wolf ||
        Component->GetWorld() != World || Wolf->GetMesh() != GetSkelMeshComponent()) return;
    const auto Current = Component->GetCastFacts();
    // 平台允许广播时同步重入发布；旧快照不能取消或开始后继攻击。
    if (Incoming.ActivationId != Current.ActivationId || Incoming.bActive != Current.bActive ||
        Incoming.AvatarGeneration != Current.AvatarGeneration || Incoming.AbilityId != Current.AbilityId ||
        Incoming.PresentationStyleId != Current.PresentationStyleId ||
        Incoming.StartedServerSeconds != Current.StartedServerSeconds || Incoming.DurationSeconds != Current.DurationSeconds)
        return;
    const bool bVampire = Wolf->IsA<ADivineBeastsVillageVampire>();
    const FName AttackId = bVampire ? FName(TEXT("VillageVampire.GreatSwordAttack")) :
        FName(TEXT("VillageWolf.Bite"));
    UAnimMontage* AttackMontage = bVampire ? GreatSwordMontage.Get() : BiteMontage.Get();
    if (Current.AbilityId != AttackId || Current.PresentationStyleId != AttackId) return;
    if (bWolfDead || Wolf->IsWolfDead() || !Current.bActive)
    {
        const bool bStopOwned = PlayingBite == Current.ActivationId;
        // 先撤销归属再调用可广播的Montage API，防止同步后继通知被外层清理覆盖。
        PlayingBite.Invalidate();
        if (bStopOwned && AttackMontage) Montage_Stop(0.05f, AttackMontage);
        return;
    }
    if (!Current.ActivationId.IsValid() || Current.ActivationId == ConsumedBite || Current.AvatarGeneration <= 0 ||
        !FMath::IsFinite(Current.StartedServerSeconds) || !FMath::IsFinite(Current.DurationSeconds) ||
        Current.DurationSeconds <= 0.0f || Current.DurationSeconds > 10.0f || !AttackMontage) return;
    const auto* State = World->GetGameState();
    const double Now = State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    if (!FMath::IsFinite(Now)) return;
    const double Elapsed = FMath::Max(0.0, Now - Current.StartedServerSeconds);
    ConsumedBite = Current.ActivationId;
    if (Elapsed >= Current.DurationSeconds) return;
    const float Length = AttackMontage->GetPlayLength();
    if (!FMath::IsFinite(Length) || Length <= 0.0f) return;
    const float Rate = Length / Current.DurationSeconds;
    const float Position = FMath::Clamp(static_cast<float>(Elapsed) * Rate, 0.0f, Length);
    // Montage开始回调也可能同步死亡、卸载或发布下一次攻击；在调用前发布归属供取消者识别。
    PlayingBite = Current.ActivationId;
    const float Played = Montage_Play(AttackMontage, Rate, EMontagePlayReturnType::MontageLength, Position, false);
    if (Played <= 0.0f && PlayingBite == Current.ActivationId) PlayingBite.Invalidate();
}
