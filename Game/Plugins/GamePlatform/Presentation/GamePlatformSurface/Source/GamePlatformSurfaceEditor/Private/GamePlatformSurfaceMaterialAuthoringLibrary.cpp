#include "GamePlatformSurfaceMaterialAuthoringLibrary.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpression.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

bool UGamePlatformSurfaceMaterialAuthoringLibrary::ConnectDisplacement(UMaterialExpression* Expression)
{
    check(IsInGameThread());
    return IsValid(Expression) &&
        UMaterialEditingLibrary::ConnectMaterialProperty(Expression, FString(), MP_Displacement);
}

TArray<UMaterialExpression*> UGamePlatformSurfaceMaterialAuthoringLibrary::GetMaterialExpressions(UObject* Asset)
{
    check(IsInGameThread());
    TArray<UMaterialExpression*> Result;
    if (const auto* Material = Cast<UMaterial>(Asset))
        for (auto Expression : Material->GetExpressions()) Result.Add(Expression);
    else if (const auto* Function = Cast<UMaterialFunction>(Asset))
        for (auto Expression : Function->GetExpressions()) Result.Add(Expression);
    return Result;
}

FString UGamePlatformSurfaceMaterialAuthoringLibrary::GetStaticMeshUVStatistics(UStaticMesh* Mesh)
{
    check(IsInGameThread());
    const FMeshDescription* Description = IsValid(Mesh) ? Mesh->GetMeshDescription(0) : nullptr;
    if (!Description) return TEXT("{\"available\":false}");
    const FStaticMeshConstAttributes Attributes(*Description);
    const auto UVs = Attributes.GetVertexInstanceUVs();
    FString Channels;
    for (int32 Channel = 0; Channel < UVs.GetNumChannels(); ++Channel)
    {
        FVector2f Minimum(MAX_flt, MAX_flt), Maximum(-MAX_flt, -MAX_flt);
        int32 NonZero = 0, Invalid = 0, InvalidU = 0, InvalidV = 0;
        for (const FVertexInstanceID Id : Description->VertexInstances().GetElementIDs())
        {
            const FVector2f Value = UVs.Get(Id, Channel);
            if (!FMath::IsFinite(Value.X)) ++InvalidU;
            if (!FMath::IsFinite(Value.Y)) ++InvalidV;
            if (Value.ContainsNaN()) { ++Invalid; continue; }
            Minimum.X = FMath::Min(Minimum.X, Value.X); Minimum.Y = FMath::Min(Minimum.Y, Value.Y);
            Maximum.X = FMath::Max(Maximum.X, Value.X); Maximum.Y = FMath::Max(Maximum.Y, Value.Y);
            if (!Value.IsNearlyZero()) ++NonZero;
        }
        Channels += FString::Printf(TEXT("%s{\"channel\":%d,\"min\":[%.9g,%.9g],\"max\":[%.9g,%.9g],\"nonzero\":%d,\"invalid\":%d,\"nonfinite_u\":%d,\"nonfinite_v\":%d}"),
            Channel ? TEXT(",") : TEXT(""), Channel, Minimum.X, Minimum.Y, Maximum.X, Maximum.Y, NonZero, Invalid, InvalidU, InvalidV);
    }
    return FString::Printf(TEXT("{\"available\":true,\"vertices\":%d,\"channels\":[%s]}"), Description->VertexInstances().Num(), *Channels);
}

AActor* UGamePlatformSurfaceMaterialAuthoringLibrary::SpawnInstancedWindReviewActor(
    UWorld* World, UStaticMesh* Mesh, const TArray<FTransform>& WorldTransforms)
{
    check(IsInGameThread());
    if (!IsValid(World) || World->WorldType != EWorldType::Editor || !IsValid(Mesh) ||
        WorldTransforms.IsEmpty() || WorldTransforms.Num() > 64) return nullptr;
    for (const auto& Transform : WorldTransforms)
        if (!Transform.IsValid()) return nullptr;
    FActorSpawnParameters Parameters; Parameters.ObjectFlags |= RF_Transactional;
    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameters);
    if (!IsValid(Actor)) return nullptr;
    auto* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, NAME_None, RF_Transactional);
    Actor->SetRootComponent(Instances); Actor->AddInstanceComponent(Instances);
    Instances->SetStaticMesh(Mesh);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instances->SetCanEverAffectNavigation(false);
    Instances->RegisterComponent();
    Instances->AddInstances(WorldTransforms, false, true, false);
    return Actor;
}
