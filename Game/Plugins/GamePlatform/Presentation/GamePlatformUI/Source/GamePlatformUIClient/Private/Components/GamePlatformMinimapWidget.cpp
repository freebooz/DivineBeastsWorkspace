#include "Components/GamePlatformMinimapWidget.h"

bool UGamePlatformMinimapWidget::ApplyMinimapState(
    const FGamePlatformUIMinimapState& InState)
{
    // 入库前一次验证，源数据非法时保持最后一个完整可用快照，不泄漏半成品。
    constexpr int32 MaxMarkers = 128;
    if (InState.MapId.IsNone() || InState.Revision < 0 ||
        InState.Markers.Num() > MaxMarkers ||
        !FMath::IsFinite(InState.PlayerNormalizedPosition.X) ||
        !FMath::IsFinite(InState.PlayerNormalizedPosition.Y) ||
        !FMath::IsFinite(InState.HeadingDegrees) ||
        !FMath::IsFinite(InState.Zoom) ||
        InState.Zoom < 0.25f || InState.Zoom > 8.0f)
    {
        return false;
    }

    if (State.MapId == InState.MapId && InState.Revision <= State.Revision)
    {
        return false;
    }

    TSet<FName> UniqueMarkers;
    UniqueMarkers.Reserve(InState.Markers.Num());
    for (const FGamePlatformUIMinimapMarker& Marker : InState.Markers)
    {
        if (Marker.MarkerId.IsNone() ||
            UniqueMarkers.Contains(Marker.MarkerId) ||
            !FMath::IsFinite(Marker.NormalizedPosition.X) ||
            !FMath::IsFinite(Marker.NormalizedPosition.Y))
        {
            return false;
        }
        UniqueMarkers.Add(Marker.MarkerId);
    }

    State = InState;
    State.PlayerNormalizedPosition.X =
        FMath::Clamp(State.PlayerNormalizedPosition.X, 0.0, 1.0);
    State.PlayerNormalizedPosition.Y =
        FMath::Clamp(State.PlayerNormalizedPosition.Y, 0.0, 1.0);
    for (FGamePlatformUIMinimapMarker& Marker : State.Markers)
    {
        Marker.NormalizedPosition.X =
            FMath::Clamp(Marker.NormalizedPosition.X, 0.0, 1.0);
        Marker.NormalizedPosition.Y =
            FMath::Clamp(Marker.NormalizedPosition.Y, 0.0, 1.0);
    }
    BP_OnMinimapChanged(State);
    return true;
}
