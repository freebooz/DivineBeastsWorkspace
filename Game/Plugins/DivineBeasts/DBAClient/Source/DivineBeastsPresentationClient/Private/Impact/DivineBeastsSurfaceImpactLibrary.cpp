// 项目层仅选表现类别并做授权前置，不直接SpawnDecal或同步加载软资源。
#include "Impact/DivineBeastsSurfaceImpactLibrary.h"
#include "Marks/GamePlatformSurfaceMarkBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"

bool UDivineBeastsSurfaceImpactLibrary::ResolveImpactVisualKind(
    FName ImpactSemantic, EGamePlatformSurfaceMarkKind& OutKind)
{
    // 这些是表现语义而非正式GAS技能ID；由已确认事实的客户端表现适配器显式选择。
    if (ImpactSemantic == TEXT("Impact.Projectile")) { OutKind = EGamePlatformSurfaceMarkKind::Bullet; return true; }
    if (ImpactSemantic == TEXT("Impact.Explosion")) { OutKind = EGamePlatformSurfaceMarkKind::Scorch; return true; }
    if (ImpactSemantic == TEXT("Impact.Slash")) { OutKind = EGamePlatformSurfaceMarkKind::Slash; return true; }
    if (ImpactSemantic == TEXT("Impact.Energy")) { OutKind = EGamePlatformSurfaceMarkKind::Fluid; return true; }
    if (ImpactSemantic == TEXT("Impact.Fracture")) { OutKind = EGamePlatformSurfaceMarkKind::Crack; return true; }
    return false;
}

FGamePlatformSurfaceMarkResult UDivineBeastsSurfaceImpactLibrary::SubmitConfirmedEnvironmentImpact(
    const UObject* WorldContextObject, const FHitResult& ConfirmedHit,
    FName ImpactSemantic, FVector AttackDirection, FGuid ConfirmedEventId, float SizeScale)
{
    FGamePlatformSurfaceMarkResult Rejected;
    Rejected.Status = EGamePlatformSurfaceMarkStatus::InvalidRequest;
    if (!ConfirmedEventId.IsValid() || !ConfirmedHit.bBlockingHit) return Rejected;
    const UPrimitiveComponent* Component = ConfirmedHit.GetComponent();
    if (!IsValid(Component)) return Rejected;
    const ECollisionChannel ObjectType = Component->GetCollisionObjectType();
    if (ObjectType != ECC_WorldStatic && ObjectType != ECC_WorldDynamic) return Rejected;
    EGamePlatformSurfaceMarkKind Kind;
    if (!ResolveImpactVisualKind(ImpactSemantic, Kind)) return Rejected;

    // 所有生命周期、物理材质、跨世界检查由平台表面服务统一处理。
    return UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMarkFromHit(
        WorldContextObject, Kind, ConfirmedHit, AttackDirection, ConfirmedEventId, SizeScale);
}
