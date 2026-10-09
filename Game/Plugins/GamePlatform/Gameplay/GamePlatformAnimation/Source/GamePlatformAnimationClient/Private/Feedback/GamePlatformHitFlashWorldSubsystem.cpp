#include "Feedback/GamePlatformHitFlashWorldSubsystem.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"

void UGamePlatformHitFlashWorldSubsystem::Deinitialize()
{
    CancelAllHitFlashes();
    Super::Deinitialize();
}

bool UGamePlatformHitFlashWorldSubsystem::PlayHitFlash(
    const FGuid& EventId,
    UMeshComponent* TargetMesh,
    UMaterialInterface* PreloadedFlashOverlay,
    float DurationSeconds)
{
    UWorld* World = GetWorld();
    if (!IsInGameThread() || !World || World->GetNetMode() == NM_DedicatedServer ||
        !EventId.IsValid() || RecentEventIds.Contains(EventId) ||
        !IsValid(TargetMesh) || TargetMesh->GetWorld() != World ||
        !IsValid(PreloadedFlashOverlay) ||
        !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
    {
        return false;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const double AllowedDuration = FMath::Clamp(
        static_cast<double>(DurationSeconds), 0.0, MaximumFlashSeconds);
    FGamePlatformHitFlashMeshRecord* Existing = ActiveRecords.FindByPredicate(
        [TargetMesh](const FGamePlatformHitFlashMeshRecord& Item)
        {
            return Item.Mesh.Get() == TargetMesh;
        });

    if (Existing)
    {
        // 已高亮中的同一网格不重新改写原Overlay，仅把到期时间裁剪至第一击上限。
        Existing->DeadlineSeconds = FMath::Max(
            Existing->DeadlineSeconds,
            FMath::Min(NowSeconds + AllowedDuration,
                Existing->FirstStartedAtSeconds + MaximumFlashSeconds));
    }
    else
    {
        if (ActiveRecords.Num() >= MaxActiveFlashes)
        {
            return false; // 高密度5v5战斗守住有界临时材料数。
        }

        FGamePlatformHitFlashMeshRecord Record;
        Record.Mesh = TargetMesh;
        Record.PreviousOverlay = TargetMesh->GetOverlayMaterial();
        Record.ActiveOverlay = PreloadedFlashOverlay;
        Record.FirstStartedAtSeconds = NowSeconds;
        Record.DeadlineSeconds = NowSeconds + AllowedDuration;
        ActiveRecords.Add(MoveTemp(Record));
        TargetMesh->SetOverlayMaterial(PreloadedFlashOverlay);
    }

    if (!ActiveTicker.IsValid())
    {
        ActiveTicker = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(
                this, &UGamePlatformHitFlashWorldSubsystem::TickHitFlashes));
    }
    RememberEvent(EventId);
    return true;
}

void UGamePlatformHitFlashWorldSubsystem::RememberEvent(const FGuid& EventId)
{
    RecentEventIds.Add(EventId);
    RecentEventOrder.Add(EventId);
    if (RecentEventOrder.Num() > MaxRecentEvents)
    {
        RecentEventIds.Remove(RecentEventOrder[0]);
        RecentEventOrder.RemoveAt(0, 1, EAllowShrinking::No);
    }
}

void UGamePlatformHitFlashWorldSubsystem::RestoreFlashAt(int32 Index)
{
    if (!ActiveRecords.IsValidIndex(Index))
    {
        return;
    }

    FGamePlatformHitFlashMeshRecord& Record = ActiveRecords[Index];
    if (UMeshComponent* Mesh = Record.Mesh.Get())
    {
        // 其他材质/角色外观系统覆盖之后不抢回Overlay所有权。
        if (Mesh->GetWorld() == GetWorld() &&
            Mesh->GetOverlayMaterial() == Record.ActiveOverlay.Get())
        {
            Mesh->SetOverlayMaterial(Record.PreviousOverlay.Get());
        }
    }
    ActiveRecords.RemoveAtSwap(Index, 1, EAllowShrinking::No);
}

bool UGamePlatformHitFlashWorldSubsystem::TickHitFlashes(float /*DeltaSeconds*/)
{
    const double NowSeconds = FPlatformTime::Seconds();
    for (int32 Index = ActiveRecords.Num() - 1; Index >= 0; --Index)
    {
        if (!ActiveRecords[Index].Mesh.IsValid() ||
            NowSeconds >= ActiveRecords[Index].DeadlineSeconds)
        {
            RestoreFlashAt(Index);
        }
    }
    if (ActiveRecords.IsEmpty())
    {
        ActiveTicker.Reset();
        return false;
    }
    return true;
}

void UGamePlatformHitFlashWorldSubsystem::CancelAllHitFlashes()
{
    if (ActiveTicker.IsValid())
    {
        FTSTicker::RemoveTicker(ActiveTicker);
        ActiveTicker.Reset();
    }
    for (int32 Index = ActiveRecords.Num() - 1; Index >= 0; --Index)
    {
        RestoreFlashAt(Index);
    }
    ActiveRecords.Reset();
    RecentEventIds.Reset();
    RecentEventOrder.Reset();
}
