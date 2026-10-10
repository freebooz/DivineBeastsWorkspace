// 项目层新手村地图显示实现；本地移动事件驱动，退出释放自有普通资源句柄，不改变真实地图或其他世界租约。
#include "Components/DivineBeastsVillageMinimapWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "UObject/Package.h"

namespace
{
/** 真实底图由Monolith导入公共UI包；此软路径不把客户端纹理带入Dedicated Server。 */
const FSoftObjectPath VillageMapTexture(TEXT("/DBAUIPack_Core/UI/Maps/T_DBA_Village_Minimap.T_DBA_Village_Minimap"));
}

bool UDivineBeastsVillageMinimapWidget::ProjectVillagePosition(const FVector& PositionCentimeters, FVector2D& OutUV)
{
    if (!FMath::IsFinite(PositionCentimeters.X) || !FMath::IsFinite(PositionCentimeters.Y)) { return false; }
    // 捕获镜头北向必须与此一致；保留Z独立，山地高度不扭曲平面地图。
    OutUV = FVector2D(FMath::Clamp((PositionCentimeters.Y + 25200.0) / 50400.0, 0.0, 1.0),
        FMath::Clamp((25200.0 - PositionCentimeters.X) / 50400.0, 0.0, 1.0));
    return true;
}

void UDivineBeastsVillageMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BindToPawn(GetOwningPlayerPawn());
}

void UDivineBeastsVillageMinimapWidget::NativeDestruct()
{
    ClearPawnBinding();
    Super::NativeDestruct();
}

void UDivineBeastsVillageMinimapWidget::BeginDestroy()
{
    // 重设蓝图父类时旧实例直接变为不可达；销毁仍取消原生请求，但禁止经ApplyMinimapState调用蓝图。
    bDestroyingNativeResources = true;
    ClearPawnBinding(false);
    Super::BeginDestroy();
}

void UDivineBeastsVillageMinimapWidget::ClearPawnBinding(bool bPublishEmptySnapshot)
{
    ++BindingGeneration;
    if (ACharacter* Character = BoundCharacter.Get())
    {
        Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
        Character->OnDestroyed.RemoveDynamic(this, &ThisClass::HandlePawnDestroyed);
    }
    BoundCharacter.Reset();
    FGamePlatformAssetLoader::Cancel(MapLoadHandle);
    MapLoadHandle.Reset();
    // 资源先清理、发布后判定；GC路径不能省掉解绑或取消，也不能修改可能已半销毁的Widget树。
    if (!bPublishEmptySnapshot || bDestroyingNativeResources || !IsValid(this) || IsUnreachable() ||
        HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)) { return; }
    // 平台快照同样撤销软引用与旧位置；取消后蓝图读取也不能再次显示上一角色。
    FGamePlatformUIMinimapState EmptySnapshot;
    EmptySnapshot.MapId = TEXT("Village.Start");
    SnapshotRevision = FMath::Max(SnapshotRevision, GetMinimapStateView().Revision) + 1;
    EmptySnapshot.Revision = SnapshotRevision;
    ApplyMinimapState(EmptySnapshot);
    // 清空画刷才真正释放Widget对旧世界普通纹理的强引用。
    if (UImage* Image = Cast<UImage>(GetWidgetFromName(TEXT("MapImage")))) { Image->SetBrushFromTexture(nullptr); }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDivineBeastsVillageMinimapWidget::BindToPawn(APawn* Pawn)
{
    if (bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    ACharacter* Character = Cast<ACharacter>(Pawn);
    if (BoundCharacter.Get() == Character && IsValid(Character)) { RefreshProjection(); return; }
    ClearPawnBinding();
    // 底图仅适用于当前真实村庄地图；旅行到其他世界不能复用村庄轮廓。
    if (!IsValid(Character) || !Character->IsLocallyControlled() || !GetWorld() ||
        !GetWorld()->GetOutermost()->GetName().EndsWith(TEXT("L_Village_Start"))) { return; }
    BoundCharacter = Character;
    Character->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::HandleMovementUpdated);
    Character->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandlePawnDestroyed);
    const uint64 ExpectedGeneration = BindingGeneration;
    TWeakObjectPtr<UDivineBeastsVillageMinimapWidget> WeakThis(this);
    MapLoadHandle = FGamePlatformAssetLoader::RequestAsyncLoad({VillageMapTexture},
        FStreamableDelegate::CreateLambda([WeakThis, ExpectedGeneration]()
        {
            if (UDivineBeastsVillageMinimapWidget* Widget = WeakThis.Get(); Widget &&
                Widget->BindingGeneration == ExpectedGeneration && Widget->BoundCharacter.IsValid())
            { Widget->RefreshProjection(); }
        }));
    RefreshProjection();
}

void UDivineBeastsVillageMinimapWidget::HandleMovementUpdated(float, FVector, FVector) { RefreshProjection(); }
void UDivineBeastsVillageMinimapWidget::HandlePawnDestroyed(AActor*) { ClearPawnBinding(); }

void UDivineBeastsVillageMinimapWidget::RefreshProjection()
{
    if (bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    ACharacter* Character = BoundCharacter.Get();
    FVector2D UV;
    if (!IsValid(Character) || Character != GetOwningPlayerPawn() ||
        !ProjectVillagePosition(Character->GetActorLocation(), UV)) { return; }
    UTexture2D* Texture = Cast<UTexture2D>(VillageMapTexture.ResolveObject());
    if (!IsValid(Texture)) { SetVisibility(ESlateVisibility::Collapsed); return; }
    const float Heading = Character->GetActorRotation().Yaw;
    if (!FMath::IsFinite(Heading)) { return; }
    const auto& Previous = GetMinimapStateView();
    if (Previous.Revision >= 0 && Previous.PlayerNormalizedPosition.Equals(UV, 0.00001) &&
        FMath::IsNearlyEqual(Previous.HeadingDegrees, Heading, 0.1f) && Previous.MapTexture.Get() == Texture)
    { SetVisibility(ESlateVisibility::SelfHitTestInvisible); return; }
    FGamePlatformUIMinimapState Snapshot;
    Snapshot.MapId = TEXT("Village.Start");
    Snapshot.Revision = ++SnapshotRevision;
    Snapshot.MapTexture = Texture;
    Snapshot.PlayerNormalizedPosition = UV;
    Snapshot.HeadingDegrees = Heading;
    if (!ApplyMinimapState(Snapshot)) { return; }
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    // 标记使用Canvas归一化锚点；UI缩放/安全区改变时由UMG布局重算，不依赖缓存像素或Tick。
    if (UWidget* Marker = GetWidgetFromName(TEXT("PlayerMarker")))
    {
        if (UCanvasPanelSlot* MarkerCanvasSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
        {
            MarkerCanvasSlot->SetAnchors(FAnchors(UV.X, UV.Y));
            MarkerCanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
            MarkerCanvasSlot->SetPosition(FVector2D::ZeroVector);
        }
        Marker->SetRenderTransformAngle(Heading);
        Marker->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}
