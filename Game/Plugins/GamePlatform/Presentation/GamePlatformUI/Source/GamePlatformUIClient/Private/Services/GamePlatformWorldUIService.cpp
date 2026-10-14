// 平台LocalPlayer世界UI：事件驱动投影和池生命周期；不查询玩法生命值，不使用Ticker。
#include "Services/GamePlatformWorldUIService.h"

#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Components/OverlaySlot.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"

namespace
{
constexpr int32 MaxActiveWorldWidgets = 128;
constexpr int32 MaxPooledWorldWidgets = 128;
}

void UGamePlatformWorldUIService::Initialize(ULocalPlayer* InLocalPlayer)
{
    LocalPlayer = InLocalPlayer;
    ActiveWidgets.Reserve(32);
    PooledWidgets.Reserve(32);

    // 空闲不订阅视口；注册、位置变化及调用方最终镜头事件显式触发投影。
}

void UGamePlatformWorldUIService::Deinitialize()
{
    UnbindProjectionEvents();
    Clear();
    RootLayout = nullptr;
    LocalPlayer = nullptr;
}

void UGamePlatformWorldUIService::SetRootLayout(
    UGamePlatformUILayerStack* InRootLayout)
{
    RootLayout = InRootLayout;
}

FGuid UGamePlatformWorldUIService::RegisterWorldUI(
    FGamePlatformWorldUIRequest Request,
    TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass)
{
    if (!Request.IsValid() ||
        !WidgetClass ||
        WidgetClass->HasAnyClassFlags(CLASS_Abstract) ||
        !IsValid(RootLayout) ||
        ActiveWidgets.Num() >= MaxActiveWorldWidgets)
    {
        return FGuid();
    }

    if (!Request.RequestId.IsValid())
    {
        Request.RequestId = FGuid::NewGuid();
    }

    if (ActiveWidgets.Contains(Request.RequestId))
    {
        return FGuid();
    }

    UGamePlatformWorldWidgetBase* Widget =
        AcquireWidget(WidgetClass);
    if (!IsValid(Widget))
    {
        return FGuid();
    }

    Widget->ApplyWorldUIRequest(Request);
    if (!RootLayout->AddWorldProjectionWidget(Widget))
    {
        RecycleWidget(Widget);
        return FGuid();
    }

    ActiveWidgets.Add(Request.RequestId, Widget);
    if (auto* Slot = Cast<UOverlaySlot>(Widget->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Left); Slot->SetVerticalAlignment(VAlign_Top); }
    BindProjectionEvents();
    RefreshProjection(Request.RequestId);
    return Request.RequestId;
}

bool UGamePlatformWorldUIService::UpdateWorldUI(
    FGuid RequestId,
    FGamePlatformWorldUIRequest Request)
{
    TWeakObjectPtr<UGamePlatformWorldWidgetBase>* WidgetPtr =
        ActiveWidgets.Find(RequestId);
    UGamePlatformWorldWidgetBase* Widget =
        WidgetPtr ? WidgetPtr->Get() : nullptr;
    if (!IsValid(Widget))
    {
        return false;
    }

    Request.RequestId = RequestId;
    Widget->ApplyWorldUIRequest(Request);
    RefreshProjection(RequestId);
    return true;
}

bool UGamePlatformWorldUIService::UnregisterWorldUI(FGuid RequestId)
{
    TWeakObjectPtr<UGamePlatformWorldWidgetBase>* WidgetPtr =
        ActiveWidgets.Find(RequestId);
    if (!WidgetPtr)
    {
        return false;
    }

    UGamePlatformWorldWidgetBase* Widget = WidgetPtr->Get();
    ActiveWidgets.Remove(RequestId);
    RecycleWidget(Widget);
    if (ActiveWidgets.IsEmpty())
    {
        UnbindProjectionEvents();
    }
    return true;
}

UGamePlatformWorldWidgetBase* UGamePlatformWorldUIService::AcquireWidget(
    TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass)
{
    for (int32 Index = PooledWidgets.Num() - 1; Index >= 0; --Index)
    {
        UGamePlatformWorldWidgetBase* Candidate =
            PooledWidgets[Index];

        if (!IsValid(Candidate))
        {
            PooledWidgets.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            continue;
        }

        if (Candidate->GetClass() == WidgetClass)
        {
            PooledWidgets.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            return Candidate;
        }
    }

    APlayerController* PlayerController =
        IsValid(LocalPlayer)
            ? LocalPlayer->GetPlayerController(GetServiceWorld())
            : nullptr;
    return IsValid(PlayerController)
        ? CreateWidget<UGamePlatformWorldWidgetBase>(
            PlayerController,
            WidgetClass)
        : nullptr;
}

void UGamePlatformWorldUIService::RecycleWidget(
    UGamePlatformWorldWidgetBase* Widget)
{
    if (!IsValid(Widget))
    {
        return;
    }

    Widget->RemoveFromParent();
    Widget->ResetWorldUIState();
    if (PooledWidgets.Num() < MaxPooledWorldWidgets)
    {
        PooledWidgets.Add(Widget);
    }
}

void UGamePlatformWorldUIService::BindProjectionEvents()
{
    if (ViewportResizedHandle.IsValid() || ActiveWidgets.IsEmpty()) return;
    ViewportResizedHandle = FViewport::ViewportResizedEvent.AddUObject(this, &ThisClass::HandleViewportResized);
}

void UGamePlatformWorldUIService::UnbindProjectionEvents()
{
    FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
    ViewportResizedHandle.Reset();
}

void UGamePlatformWorldUIService::HandleViewportResized(FViewport* Viewport, uint32)
{
    if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport == Viewport)
        RefreshProjection();
}

void UGamePlatformWorldUIService::RefreshProjection(FGuid OnlyRequestId)
{
    APlayerController* PlayerController =
        IsValid(LocalPlayer)
            ? LocalPlayer->GetPlayerController(GetServiceWorld())
            : nullptr;
    if (!IsValid(PlayerController))
    {
        return;
    }

    const FVector CameraLocation =
        IsValid(PlayerController->PlayerCameraManager)
            ? PlayerController->PlayerCameraManager->GetCameraLocation()
            : FVector::ZeroVector;

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;
    PlayerController->GetViewportSize(
        ViewportWidth,
        ViewportHeight);

    TArray<FGuid> StaleIds;
    // 投影通知可能同步注销；遍历身份快照并每次重新查弱引用，避免迭代器失效。
    TArray<FGuid> Ids; ActiveWidgets.GetKeys(Ids);
    for (const FGuid& Id : Ids)
    {
        if (OnlyRequestId.IsValid() && Id != OnlyRequestId) continue;
        const auto* WeakWidget = ActiveWidgets.Find(Id);
        UGamePlatformWorldWidgetBase* Widget = WeakWidget ? WeakWidget->Get() : nullptr;
        if (!IsValid(Widget))
        {
            StaleIds.Add(Id);
            continue;
        }

        const FGamePlatformWorldUIRequest& Request =
            Widget->GetWorldUIRequest();
        const float Distance =
            FVector::Distance(
                CameraLocation,
                Request.WorldLocation);

        bool bVisible = Request.bDisplayRequested &&
            (Request.MaxVisibleDistance <= 0.0f || Distance <= Request.MaxVisibleDistance);

        FVector2D ScreenPosition = FVector2D::ZeroVector;
        if (bVisible)
        {
            bVisible =
                PlayerController->ProjectWorldLocationToScreen(
                    Request.WorldLocation,
                    ScreenPosition,
                    true);
        }

        if (bVisible &&
            Request.bClampToViewport &&
            ViewportWidth > 0 &&
            ViewportHeight > 0)
        {
            ScreenPosition.X = FMath::Clamp(
                ScreenPosition.X,
                0.0,
                static_cast<double>(ViewportWidth));
            ScreenPosition.Y = FMath::Clamp(
                ScreenPosition.Y,
                0.0,
                static_cast<double>(ViewportHeight));
        }

        Widget->SetProjectedState(
            ScreenPosition,
            bVisible,
            Distance);
    }

    for (const FGuid& StaleId : StaleIds)
    {
        ActiveWidgets.Remove(StaleId);
    }

    if (ActiveWidgets.IsEmpty()) UnbindProjectionEvents();
}

void UGamePlatformWorldUIService::Clear()
{
    TArray<FGuid> RequestIds;
    ActiveWidgets.GetKeys(RequestIds);
    for (const FGuid& RequestId : RequestIds)
    {
        UnregisterWorldUI(RequestId);
    }
    ActiveWidgets.Reset();
    UnbindProjectionEvents();

    for (UGamePlatformWorldWidgetBase* Widget : PooledWidgets)
    {
        if (IsValid(Widget))
        {
            Widget->RemoveFromParent();
            Widget->ResetWorldUIState();
        }
    }
    PooledWidgets.Reset();
}

UWorld* UGamePlatformWorldUIService::GetServiceWorld() const
{
    return IsValid(LocalPlayer) ? LocalPlayer->GetWorld() : nullptr;
}
