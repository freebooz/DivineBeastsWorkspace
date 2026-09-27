#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"

void UDivineBeastsLoadingViewModel::InitializeLoadingService(
    UGamePlatformLoadingScreenService* InLoadingService)
{
    if (LoadingService == InLoadingService)
    {
        return;
    }

    const bool bWasBound = bLoadingEventsBound;
    if (bWasBound)
    {
        UnbindLoadingEvents();
    }

    LoadingService = InLoadingService;

    if (bWasBound && IsPageActive())
    {
        BindLoadingEvents();
        LoadingSnapshot = IsValid(LoadingService)
            ? LoadingService->GetSnapshot()
            : FGamePlatformUILoadingSnapshot();
        MarkStateChanged();
    }
}

void UDivineBeastsLoadingViewModel::OnPageBegan()
{
    Super::OnPageBegan();

    BindLoadingEvents();
    LoadingSnapshot = IsValid(LoadingService)
        ? LoadingService->GetSnapshot()
        : FGamePlatformUILoadingSnapshot();

    // BeginPage 会在本扩展点返回后统一广播一次初始状态，因此这里不重复广播。
}

void UDivineBeastsLoadingViewModel::OnPageEnded()
{
    UnbindLoadingEvents();
    Super::OnPageEnded();
}

void UDivineBeastsLoadingViewModel::HandleLoadingSnapshotChanged(
    const FGamePlatformUILoadingSnapshot& Snapshot)
{
    if (AreSnapshotsEquivalent(LoadingSnapshot, Snapshot))
    {
        return;
    }

    LoadingSnapshot = Snapshot;
    MarkStateChanged();
}

void UDivineBeastsLoadingViewModel::BindLoadingEvents()
{
    if (bLoadingEventsBound || !IsValid(LoadingService))
    {
        return;
    }

    LoadingService->OnSnapshotChanged.AddUniqueDynamic(
        this,
        &UDivineBeastsLoadingViewModel::HandleLoadingSnapshotChanged);
    bLoadingEventsBound = true;
}

void UDivineBeastsLoadingViewModel::UnbindLoadingEvents()
{
    if (!bLoadingEventsBound)
    {
        return;
    }

    if (IsValid(LoadingService))
    {
        LoadingService->OnSnapshotChanged.RemoveDynamic(
            this,
            &UDivineBeastsLoadingViewModel::HandleLoadingSnapshotChanged);
    }

    bLoadingEventsBound = false;
}

bool UDivineBeastsLoadingViewModel::AreSnapshotsEquivalent(
    const FGamePlatformUILoadingSnapshot& A,
    const FGamePlatformUILoadingSnapshot& B)
{
    return A.bIsLoading == B.bIsLoading &&
        A.ActiveTokenCount == B.ActiveTokenCount &&
        A.Stage.EqualTo(B.Stage) &&
        FMath::IsNearlyEqual(A.Progress, B.Progress, 0.0001f);
}
