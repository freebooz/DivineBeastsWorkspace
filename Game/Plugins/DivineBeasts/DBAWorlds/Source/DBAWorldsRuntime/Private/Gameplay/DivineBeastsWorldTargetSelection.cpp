// 目标候选只在明确TAB输入时查询；不扫描Tick、不授予交互/施法资格，不修改在途弹体已冻结的原目标。
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

void ADivineBeastsWorldPlayerController::RequestNextWorldTarget()
{
    UWorld* World = GetWorld();
    APawn* Source = GetPawn();
    if (!CanProcessWorldInput() || !World || World->bIsTearingDown || World->IsPaused() || !IsValid(Source)) return;
    // 键盘选择取代未完成的鼠标点击意图；保留RMB观察/转向，松开LMB不能恢复按下时的旧目标。
    bPointerClickPending = false;
    PointerPressedTarget.Reset();

    struct FCandidate { AActor* Actor; double DistanceSquared; FString StablePath; };
    TArray<FCandidate> Candidates;
    FVector ViewLocation; FRotator ViewRotation;
    GetPlayerViewPoint(ViewLocation, ViewRotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DivineBeastsTabTarget), false, Source);
    constexpr double MaximumDistanceSquared = 10000.0 * 10000.0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || Actor == Source || Actor->IsActorBeingDestroyed() || Actor->IsHidden() ||
            (!Cast<APawn>(Actor) && !Actor->FindComponentByClass<UGamePlatformInteractableComponent>())) continue;
        const auto* Combat = Actor->FindComponentByClass<UGamePlatformCombatComponent>();
        if (Combat && (Combat->IsCombatDead() || (Combat->GetCombatMaxHealth() > 0.f && Combat->GetCombatHealth() <= 0.f))) continue;
        const FBox CollisionBounds = Actor->GetComponentsBoundingBox(true);
        const FVector Center = Combat ? Combat->GetCombatTargetPoint() :
            (CollisionBounds.IsValid ? CollisionBounds.GetCenter() : Actor->GetActorLocation());
        const double DistanceSquared = FVector::DistSquared(Source->GetActorLocation(), Center);
        if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > MaximumDistanceSquared) continue;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, ViewLocation, Center, ECC_Visibility, Query) && Hit.GetActor() != Actor) continue;
        Candidates.Add({Actor, DistanceSquared, Actor->GetPathName()});
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B)
    {
        // 精确距离比较保持严格弱序；同距使用稳定路径，不能让Actor迭代/哈希顺序决定目标。
        return A.DistanceSquared != B.DistanceSquared ? A.DistanceSquared < B.DistanceSquared :
            A.StablePath.Compare(B.StablePath, ESearchCase::CaseSensitive) < 0;
    });
    if (Candidates.IsEmpty()) { SetSelectedWorldTarget(nullptr); return; }
    const int32 Current = Candidates.IndexOfByPredicate([this](const FCandidate& Entry) { return Entry.Actor == SelectedWorldTarget.Get(); });
    SetSelectedWorldTarget(Candidates[(Current + 1) % Candidates.Num()].Actor);
}
