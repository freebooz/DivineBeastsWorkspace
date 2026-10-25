#include "API/GamePlatformSurfaceWindPreview.h"
#include "Validation/GamePlatformSurfaceAssetContract.h"
#include "Types/GamePlatformSurfaceParameterNames.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

namespace
{
UMaterialParameterCollectionInstance* Resolve(UWorld& World, FString& Error)
{
    check(IsInGameThread()); Error.Reset();
    if (World.WorldType != EWorldType::Editor || World.bIsTearingDown || IsRunningCommandlet())
    { Error = TEXT("EditorWindPreviewRequiresEditorWorld"); return nullptr; }
    auto* Collection = Cast<UMaterialParameterCollection>(FGamePlatformSurfaceAssetContract::GetGlobalParameterCollectionObjectPath().TryLoad());
    if (!IsValid(Collection) || !FGamePlatformSurfaceAssetContract::ValidateGlobalParameterCollection(*Collection, Error)) return nullptr;
    return World.GetParameterCollectionInstance(Collection);
}
FVector4 Vector(const FLinearColor& C) { return FVector4(C.R, C.G, C.B, C.A); }
FLinearColor Color(const FVector4& V) { return FLinearColor(V.X, V.Y, V.Z, V.W); }
}
bool GamePlatformSurfaceWindPreview::Read(UWorld& World, FGamePlatformSurfaceWindTransition& Wind, FString& Error)
{
    auto* Instance = Resolve(World, Error); if (!Instance) return false;
    FLinearColor From, To, Timing; float Seed = 0.f;
    using namespace GamePlatformSurfaceParameters;
    if (!Instance->GetVectorParameterValue(GlobalWindFrom, From) || !Instance->GetVectorParameterValue(GlobalWindTo, To) ||
        !Instance->GetVectorParameterValue(GlobalWindTiming, Timing) || !Instance->GetScalarParameterValue(GlobalWindSeed, Seed)) return false;
    Wind.From = Vector(From); Wind.To = Vector(To); Wind.Timing = Vector(Timing); Wind.Seed = FMath::RoundToInt(Seed);
    return Wind.IsValid();
}
bool GamePlatformSurfaceWindPreview::Apply(UWorld& World, const FGamePlatformSurfaceWindTransition& Wind, FString& Error)
{
    if (!Wind.IsValid()) { Error = TEXT("InvalidEditorWindPreview"); return false; }
    auto* Instance = Resolve(World, Error); if (!Instance) return false;
    using namespace GamePlatformSurfaceParameters;
    Instance->SetVectorParameterValue(GlobalWindFrom, Color(Wind.From));
    Instance->SetVectorParameterValue(GlobalWindTo, Color(Wind.To));
    Instance->SetVectorParameterValue(GlobalWindTiming, Color(Wind.Timing));
    Instance->SetScalarParameterValue(GlobalWindSeed, Wind.Seed);
    return true;
}
