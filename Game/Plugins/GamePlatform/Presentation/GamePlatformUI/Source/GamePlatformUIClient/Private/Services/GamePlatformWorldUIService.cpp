#include "Services/GamePlatformWorldUIService.h"

#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"

namespace
{
constexpr int32 MaxActiveWorldWidgets = 128;
constexpr int32 MaxPooledWorldWidgets = 128;
constexpr float ProjectionIntervalSeconds = 1.0f / 30.0f;
}

void UGamePlatformWorldUIService::Initialize(ULocalPlayer* InLocalPlayer)
{
    LocalPlayer = InLocalPlayer;
    ActiveWidgets.Reserve(32);
    PooledWidgets.Reserve(32);

    // 空闲时不注册Ticker；第一个世界UI实例注册成功后再按需启动。
}

void UGamePlatformWorldUIService::Deinitialize()
{
    StopProjectionTicker();
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
    EnsureProjectionTicker();
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
        StopProjectionTicker();
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

void UGamePlatformWorldUIService::EnsureProjectionTicker()
{
    if (ProjectionTickerHandle.IsValid() ||
        ActiveWidgets.IsEmpty())
    {
        return;
    }

    ProjectionTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(
            this,
            &UGamePlatformWorldUIService::TickProjection),
        ProjectionIntervalSeconds);
}

void UGamePlatformWorldUIService::StopProjectionTicker()
{
    if (!ProjectionTickerHandle.IsValid())
    {
        return;
    }

    FTSTicker::GetCoreTicker().RemoveTicker(
        ProjectionTickerHandle);
    ProjectionTickerHandle.Reset();
}

bool UGamePlatformWorldUIService::TickProjection(float DeltaSeconds)
{
    APlayerController* PlayerController =
        IsValid(LocalPlayer)
            ? LocalPlayer->GetPlayerController(GetServiceWorld())
            : nullptr;
    if (!IsValid(PlayerController))
    {
        return true;
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
    for (TPair<FGuid, TWeakObjectPtr<UGamePlatformWorldWidgetBase>>& Pair :
         ActiveWidgets)
    {
        UGamePlatformWorldWidgetBase* Widget = Pair.Value.Get();
        if (!IsValid(Widget))
        {
            StaleIds.Add(Pair.Key);
            continue;
        }

        const FGamePlatformWorldUIRequest& Request =
            Widget->GetWorldUIRequest();
        const float Distance =
            FVector::Distance(
                CameraLocation,
                Request.WorldLocation);

        bool bVisible =
            Request.MaxVisibleDistance <= 0.0f ||
            Distance <= Request.MaxVisibleDistance;

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

    // 仅剩失效弱引用时在当前回调结束后自动停止Ticker。
    if (ActiveWidgets.IsEmpty())
    {
        ProjectionTickerHandle.Reset();
        return false;
    }

    return true;
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
    StopProjectionTicker();

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
