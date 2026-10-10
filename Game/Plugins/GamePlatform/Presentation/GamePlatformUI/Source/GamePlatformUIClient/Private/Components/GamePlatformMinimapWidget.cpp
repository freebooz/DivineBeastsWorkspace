#include "Components/GamePlatformMinimapWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

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
    // 项目数据服务需预先异步加载MapTexture（地图底图），平台UI不阻塞同步加载或扫描Actor。
    if (UImage* Image = Cast<UImage>(GetWidgetFromName(TEXT("MapImage"))))
    {
        UTexture2D* Loaded = State.MapTexture.Get();
        Image->SetBrushFromTexture(Loaded);
        const bool bMapReady = IsValid(Loaded);
        SetVisibility(bMapReady
            ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Collapsed);

        if (UTextBlock* Marker = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlayerMarker"))))
        {
            Marker->SetVisibility(bMapReady
                ? ESlateVisibility::SelfHitTestInvisible
                : ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* MapSlot = Cast<UCanvasPanelSlot>(Image->Slot))
            {
                if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
                {
                    const FVector2D Position = MapSlot->GetPosition();
                    const FVector2D Size = MapSlot->GetSize();
                    MarkerSlot->SetPosition(Position + FVector2D(
                        State.PlayerNormalizedPosition.X * Size.X,
                        State.PlayerNormalizedPosition.Y * Size.Y));
                }
            }
        }
    }
    // 最终显示对象与标记必须来自经授权的地图快照；UI计算坐标不影响Gameplay真值。
    BP_OnMinimapChanged(State);
    return true;
}
