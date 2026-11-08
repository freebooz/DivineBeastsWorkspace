#pragma once
#include "CoreMinimal.h"
#include "GamePlatformUITypes.h"
#include "Input/GamePlatformUIWindowDragSettings.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Components/SlateWrapperTypes.h"
#include "GamePlatformUIWindowDrag.generated.h"

class UUserWidget;
class UWidget;

/** 两个既有 UI 基类共用的唯一拖动实现；每 Widget 实例持有，不建立全局输入管理器。 */
UCLASS()
class UGamePlatformUIWindowDrag final : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(UUserWidget* InOwner, const FGamePlatformUIWindowDragSettings& InSettings);
    FReply MouseDown(const FGeometry& Geometry, const FPointerEvent& Event);
    FReply MouseMove(const FGeometry& Geometry, const FPointerEvent& Event);
    FReply MouseUp(const FPointerEvent& Event);
    void CaptureLost(const FCaptureLostEvent& Event);
    void Cancel();
    void Shutdown();
    void Adapt(const FGamePlatformUIAdaptiveContext& Context);
    /** 生命周期/视口事件请求的一次布局收敛；有效布局通知后不持续查询。 */
    void LayoutReady(const FGeometry& Geometry);

protected:
    virtual void BeginDestroy() override;

private:
    struct FTarget
    {
        TWeakObjectPtr<UWidget> Widget;
        FVector2D OriginalTranslation = FVector2D::ZeroVector;
        FVector2D StartTranslation = FVector2D::ZeroVector;
        float LayoutScale = 1.f;
    };
    bool ResolveTargets();
    bool ResolveViewport(const FGeometry& Geometry, FSlateRect& OutBounds) const;
    void ResetPosition();
    void VisibilityChanged(ESlateVisibility Visibility);
    void ApplicationActivationChanged(bool bActive);
    bool Matches(const FPointerEvent& Event) const;

    TWeakObjectPtr<UUserWidget> Owner;
    TWeakObjectPtr<UWidget> Bounds;
    TWeakPtr<SWidget> CapturedSlate;
    FGamePlatformUIWindowDragSettings Settings;
    TArray<FTarget> Targets;
    FDelegateHandle VisibilityHandle;
    FDelegateHandle ApplicationHandle;
    FGamePlatformUIAdaptiveContext AdaptiveContext;
    bool bHasAdaptiveContext = false;
    bool bDragging = false;
    bool bPendingLayoutFit = false;
    uint32 UserIndex = 0;
    uint32 PointerIndex = 0;
    FVector2D StartPointer = FVector2D::ZeroVector;
    FSlateRect StartBounds;
    FSlateRect ViewportBounds;
};
