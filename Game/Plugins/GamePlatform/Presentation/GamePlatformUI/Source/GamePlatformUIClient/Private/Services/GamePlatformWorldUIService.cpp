// 平台LocalPlayer世界UI：事件驱动投影和池生命周期；不查询玩法生命值，不使用Ticker。
#include "Services/GamePlatformWorldUIService.h"

#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Components/OverlaySlot.h"
#include "UObject/StrongObjectPtr.h"
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
    RootLayoutChanged.Clear(); ++RootLayoutGeneration;
}

void UGamePlatformWorldUIService::SetRootLayout(
    UGamePlatformUILayerStack* InRootLayout)
{
    if (RootLayout == InRootLayout) return;
    const uint64 Generation = ++RootLayoutGeneration;
    TStrongObjectPtr<UGamePlatformUILayerStack> NewRoot(InRootLayout);
    RootLayout = InRootLayout;
    // Construct可能在服务获知新Root前将实例挂到旧层。只迁移入口时已有身份；
    // 强引用保留Widget，外部解绑/换Root后的旧栈不能复活已取消的实例。
    TArray<TPair<FGuid, uint64>> Registrations;
    Registrations.Reserve(ActiveWidgets.Num());
    for (const auto& Pair : ActiveWidgets) Registrations.Emplace(Pair.Key, Pair.Value.Registration);
    for (const auto& Pair : Registrations)
    {
        if (Generation != RootLayoutGeneration || RootLayout != NewRoot.Get()) return;
        const auto* Active = FindRegistration(Pair.Key, Pair.Value);
        if (!Active) continue;
        TStrongObjectPtr<UGamePlatformWorldWidgetBase> Widget(Active->Widget.Get());
        if (!Widget.IsValid()) { ActiveWidgets.Remove(Pair.Key); continue; }
        Widget->RemoveFromParent();
        if (Generation != RootLayoutGeneration || RootLayout != NewRoot.Get()) return;
        if (!FindRegistration(Pair.Key, Pair.Value)) continue;
        const bool bAttached = NewRoot.IsValid() && NewRoot->AddWorldProjectionWidget(Widget.Get());
        // AddChild也可能同步触发用户回调；先核对代次和身份，再操作当前槽或回收失败实例。
        if (Generation != RootLayoutGeneration || RootLayout != NewRoot.Get()) return;
        if (!FindRegistration(Pair.Key, Pair.Value)) continue;
        if (!bAttached) { ActiveWidgets.Remove(Pair.Key); RecycleWidget(Widget.Get()); continue; }
        if (auto* Slot = Cast<UOverlaySlot>(Widget->Slot))
        { Slot->SetHorizontalAlignment(HAlign_Left); Slot->SetVerticalAlignment(VAlign_Top); }
    }
    if (Generation != RootLayoutGeneration || RootLayout != NewRoot.Get()) return;
    RefreshProjection();
    if (Generation == RootLayoutGeneration && RootLayout == NewRoot.Get()) RootLayoutChanged.Broadcast();
}

FGuid UGamePlatformWorldUIService::RegisterWorldUI(
    FGamePlatformWorldUIRequest Request,
    TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass)
{
    if (bClearing || !Request.IsValid() ||
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

    const uint64 Generation = RootLayoutGeneration;
    TStrongObjectPtr<UGamePlatformUILayerStack> Root(RootLayout.Get());
    const auto CanAttach = [&]()
    {
        return Generation == RootLayoutGeneration && IsValid(Root.Get()) && RootLayout == Root.Get() &&
            ActiveWidgets.Num() < MaxActiveWorldWidgets && !ActiveWidgets.Contains(Request.RequestId);
    };
    if (!CanAttach()) return FGuid();

    TStrongObjectPtr<UGamePlatformWorldWidgetBase> Widget(AcquireWidget(WidgetClass));
    if (!Widget.IsValid()) return FGuid();

    if (CanAttach()) Widget->ApplyWorldUIRequest(Request);
    if (!CanAttach() || !Root->AddWorldProjectionWidget(Widget.Get()) || !CanAttach())
    {
        RecycleWidget(Widget.Get(), Generation == RootLayoutGeneration);
        return FGuid();
    }

    const uint64 Registration = ++NextRegistration;
    ActiveWidgets.Add(Request.RequestId, FActiveWorldWidget{Widget.Get(), Registration});
    if (auto* Slot = Cast<UOverlaySlot>(Widget->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Left); Slot->SetVerticalAlignment(VAlign_Top); }
    BindProjectionEvents();
    RefreshProjection(Request.RequestId);
    return FindRegistration(Request.RequestId, Registration) ? Request.RequestId : FGuid();
}

bool UGamePlatformWorldUIService::UpdateWorldUI(
    FGuid RequestId,
    FGamePlatformWorldUIRequest Request)
{
    const auto* Active = ActiveWidgets.Find(RequestId);
    TStrongObjectPtr<UGamePlatformWorldWidgetBase> Widget(Active ? Active->Widget.Get() : nullptr);
    if (!Widget.IsValid())
    {
        return false;
    }

    const uint64 Registration = Active->Registration;
    const uint64 Generation = RootLayoutGeneration;
    Request.RequestId = RequestId;
    Widget->ApplyWorldUIRequest(Request);
    if (Generation == RootLayoutGeneration && FindRegistration(RequestId, Registration))
        RefreshProjection(RequestId);
    return true;
}

bool UGamePlatformWorldUIService::UpdateWorldUILocation(FGuid RequestId, const FVector& WorldLocation)
{
    const auto* Active = ActiveWidgets.Find(RequestId);
    auto* Widget = Active ? Active->Widget.Get() : nullptr;
    if (!IsValid(Widget)) return false;

    Widget->UpdateWorldUILocation(WorldLocation);
    RefreshProjection(RequestId);
    return true;
}

bool UGamePlatformWorldUIService::UnregisterWorldUI(FGuid RequestId)
{
    FActiveWorldWidget Active;
    if (!ActiveWidgets.RemoveAndCopyValue(RequestId, Active)) return false;

    RecycleWidget(Active.Widget.Get());
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
    UGamePlatformWorldWidgetBase* Widget, bool bAllowPooling)
{
    TStrongObjectPtr<UGamePlatformWorldWidgetBase> RetainedWidget(Widget);
    if (!RetainedWidget.IsValid()) return;
    const uint64 Generation = RootLayoutGeneration;
    Widget->RemoveFromParent();
    Widget->ResetWorldUIState();
    if (bAllowPooling && Generation == RootLayoutGeneration && PooledWidgets.Num() < MaxPooledWorldWidgets)
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
    const uint64 Generation = RootLayoutGeneration;
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

    FIntPoint ViewportSize = FIntPoint::ZeroValue;
    PlayerController->GetViewportSize(ViewportSize.X, ViewportSize.Y);

    if (OnlyRequestId.IsValid())
    {
        if (const auto* Active = ActiveWidgets.Find(OnlyRequestId))
            ProjectWidget(OnlyRequestId, *Active, *PlayerController, CameraLocation, ViewportSize, Generation);
    }
    else
    {
        // 同步投影通知可能注销并立即用相同ID及池对象重新注册。快照只处理入口时的注册代次。
        TArray<TPair<FGuid, uint64>> Registrations;
        Registrations.Reserve(ActiveWidgets.Num());
        for (const auto& Pair : ActiveWidgets) Registrations.Emplace(Pair.Key, Pair.Value.Registration);
        for (const auto& Pair : Registrations)
        {
            if (Generation != RootLayoutGeneration || !IsValid(PlayerController)) break;
            if (const auto* Active = FindRegistration(Pair.Key, Pair.Value))
                ProjectWidget(Pair.Key, *Active, *PlayerController, CameraLocation, ViewportSize, Generation);
        }
    }

    if (ActiveWidgets.IsEmpty()) UnbindProjectionEvents();
}

void UGamePlatformWorldUIService::ProjectWidget(FGuid RequestId, FActiveWorldWidget Active,
    const APlayerController& PlayerController, const FVector& CameraLocation, FIntPoint ViewportSize, uint64 LayoutGeneration)
{
    TStrongObjectPtr<UGamePlatformWorldWidgetBase> Widget(Active.Widget.Get());
    if (!Widget.IsValid())
    {
        ActiveWidgets.Remove(RequestId);
        return;
    }

    const auto& Request = Widget->GetWorldUIRequest();
    const FVector WorldLocation = Request.WorldLocation;
    const float Distance = FVector::Distance(CameraLocation, WorldLocation);
    const bool bClampToViewport = Request.bClampToViewport;
    bool bVisible = Request.bDisplayRequested &&
        (Request.MaxVisibleDistance <= 0.0f || Distance <= Request.MaxVisibleDistance);
    FVector2D ScreenPosition = FVector2D::ZeroVector;
    if (bVisible)
    {
        const uint64 Revision = Widget->ProjectionRevision;
        bVisible = PlayerController.ProjectWorldLocationToScreen(WorldLocation, ScreenPosition, true);
        // PostProcessWorldToScreen允许调用方覆盖，回来后不能写入已注销或换Root的控件。
        if (LayoutGeneration != RootLayoutGeneration || Widget->ProjectionRevision != Revision ||
            !FindRegistration(RequestId, Active.Registration)) return;
    }
    if (bVisible && bClampToViewport && ViewportSize.X > 0 && ViewportSize.Y > 0)
    {
        ScreenPosition.X = FMath::Clamp(ScreenPosition.X, 0.0, static_cast<double>(ViewportSize.X));
        ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, 0.0, static_cast<double>(ViewportSize.Y));
    }
    Widget->SetProjectedState(ScreenPosition, bVisible, Distance);
}

const UGamePlatformWorldUIService::FActiveWorldWidget* UGamePlatformWorldUIService::FindRegistration(
    FGuid RequestId, uint64 Registration) const
{
    const auto* Active = ActiveWidgets.Find(RequestId);
    return Active && Active->Registration == Registration ? Active : nullptr;
}

void UGamePlatformWorldUIService::Clear()
{
    if (bClearing) return;
    TGuardValue<bool> Clearing(bClearing, true);
    ++RootLayoutGeneration;
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
