#include "Services/GamePlatformPCGInspection.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "PCGComponent.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGManagedResource.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Data/PCGBasePointData.h"
#include "Misc/SecureHash.h"

namespace
{
FGamePlatformResult Rejected(FName Code)
{ return FGamePlatformResult::Failure(Code,TEXT("PCG图或输出不符合当前有限白名单；禁止继续生成/交付")); }
UPCGNode* ExactNode(UPCGGraph& Graph,UClass* Type)
{
    UPCGNode* Found = nullptr;
    for (auto* Node : Graph.GetNodes())
    {
        if (Node && Node->GetSettings() && Node->GetSettings()->GetClass() == Type)
        { if (Found) { return nullptr; } Found = Node; }
    }
    return Found;
}
bool SoleEdge(const UPCGNode& From,const UPCGNode& To,FName TargetPin = PCGPinConstants::DefaultInputLabel)
{
    const auto* Pin = From.GetOutputPin(PCGPinConstants::DefaultOutputLabel);
    const auto* Input = To.GetInputPin(TargetPin);
    return Pin && Input && Pin->Edges.Num() == 1 && Input->Edges.Num() == 1 && Pin->Edges[0] &&
        Pin->Edges[0]->InputPin == Pin && Pin->Edges[0]->OutputPin == Input && Input->Edges[0] == Pin->Edges[0];
}
}

FGamePlatformResult GamePlatformPCGInspection::ValidateApprovedGraph(const UGamePlatformPCGProfileDefinition& Profile)
{
    check(IsInGameThread()); const auto Valid = Profile.ValidateDefinition(); if (!Valid.IsSuccess()) { return Valid; }
    auto* Graph = Profile.GraphReference.Get(); auto* Mesh = Profile.OutputMesh.Get();
    if (!Graph || !Mesh) { return Rejected(TEXT("ProfileAssetsNotLeased")); }
    if (Graph->IsHierarchicalGenerationEnabled() || Graph->GetNodes().Num() != 4) { return Rejected(TEXT("UnsupportedGraphShape")); }
    auto* Grid = ExactNode(*Graph,UPCGCreatePointsGridSettings::StaticClass());
    auto* Transform = ExactNode(*Graph,UPCGTransformPointsSettings::StaticClass());
    auto* Filter = ExactNode(*Graph,UPCGDensityFilterSettings::StaticClass());
    auto* Spawner = ExactNode(*Graph,UPCGStaticMeshSpawnerSettings::StaticClass());
    if (!Grid || !Transform || !Filter || !Spawner || !Graph->GetOutputNode() ||
        !SoleEdge(*Grid,*Transform) || !SoleEdge(*Transform,*Filter) || !SoleEdge(*Filter,*Spawner) || !SoleEdge(*Spawner,*Graph->GetOutputNode(),PCGPinConstants::DefaultOutputLabel))
    { return Rejected(TEXT("UnapprovedNodeOrEdge")); }
    for (auto* Node : Graph->GetNodes())
    {
        if (!Node->GetSettings()->bEnabled || Node->GetSettings()->ShouldExecuteOnGPU() || Node->GetSettingsInterface() != Node->GetSettings())
        { return Rejected(TEXT("UnapprovedExecutionMode")); }
        for (const auto& Pin : Node->GetInputPins())
        { if (Pin && (Node == Grid || Pin->Properties.Label != PCGPinConstants::DefaultInputLabel) && !Pin->Edges.IsEmpty()) { return Rejected(TEXT("ParameterConnectionForbidden")); } }
        for (const auto& Pin : Node->GetOutputPins())
        { if (Pin && Pin->Properties.Label != PCGPinConstants::DefaultOutputLabel && !Pin->Edges.IsEmpty()) { return Rejected(TEXT("ExtraOutputForbidden")); } }
    }
    for (const auto& Pin : Graph->GetOutputNode()->GetInputPins())
    { if (Pin && Pin->Properties.Label != PCGPinConstants::DefaultOutputLabel && !Pin->Edges.IsEmpty()) { return Rejected(TEXT("ExtraGraphOutputForbidden")); } }
    auto* Settings = CastChecked<UPCGStaticMeshSpawnerSettings>(Spawner->GetSettings());
    auto* Selector = Cast<UPCGMeshSelectorWeighted>(Settings->MeshSelectorParameters);
    if (!Selector || Selector->GetClass() != UPCGMeshSelectorWeighted::StaticClass() || Selector->MeshEntries.Num() != 1 ||
        Selector->bUseAttributeMaterialOverrides || !Settings->PostProcessFunctionNames.IsEmpty() || !Settings->TargetActor.IsNull() ||
        !Settings->StaticMeshComponentPropertyOverrides.IsEmpty() || Settings->InstancePackerParameters || Settings->InstancePackerType ||
        Settings->MeshSelectorType != UPCGMeshSelectorWeighted::StaticClass() || !Selector->MaterialOverrideAttributes.IsEmpty())
    { return Rejected(TEXT("SpawnerSideEffectsForbidden")); }
    const auto& Descriptor = Selector->MeshEntries[0].Descriptor;
    if (Descriptor.StaticMesh.ToSoftObjectPath() != Profile.OutputMesh.ToSoftObjectPath() || !Descriptor.OverrideMaterials.IsEmpty() ||
        Descriptor.ComponentClass != UInstancedStaticMeshComponent::StaticClass() || Selector->MeshEntries[0].Weight <= 0)
    { return Rejected(TEXT("UnapprovedOutputType")); }
    if (Profile.OutputUsage == EGamePlatformPCGOutputUsage::Cosmetic &&
        (Descriptor.bUseDefaultCollision || Descriptor.BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
         Descriptor.bGenerateOverlapEvents || Descriptor.bCanEverAffectNavigation)) { return Rejected(TEXT("CosmeticGameplayEffect")); }
    if (Profile.OutputUsage == EGamePlatformPCGOutputUsage::StaticCollision &&
        Descriptor.BodyInstance.GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics) { return Rejected(TEXT("StaticCollisionMissing")); }
    return FGamePlatformResult::Success();
}

FGamePlatformResult GamePlatformPCGInspection::ConfigureOwnedComponent(UPCGComponent& Component,
    const UGamePlatformPCGProfileDefinition& Profile,int32 DerivedSeed)
{
    check(IsInGameThread()); const auto Valid = ValidateApprovedGraph(Profile); if (!Valid.IsSuccess()) { return Valid; }
    if (Component.IsGenerating() || Component.IsCleaningUp() || Component.IsPartitioned()) { return Rejected(TEXT("ComponentBusyOrPartitioned")); }
    auto* Graph = DuplicateObject<UPCGGraph>(Profile.GraphReference.Get(),&Component);
    Graph->SetFlags(RF_Transient);
    auto* Grid = CastChecked<UPCGCreatePointsGridSettings>(ExactNode(*Graph,UPCGCreatePointsGridSettings::StaticClass())->GetSettings());
    Grid->GridExtents = FVector(Profile.HalfExtentCm.X,Profile.HalfExtentCm.Y,0);
    const double Spacing = Profile.Density > 0 ? Profile.SpacingCm / FMath::Sqrt(Profile.Density) : Profile.SpacingCm;
    Grid->CellSize = FVector(Spacing,Spacing,100); Grid->CoordinateSpace = EPCGCoordinateSpace::OriginalComponent;
    Grid->bCullPointsOutsideVolume = false; Grid->PointPosition = EPCGPointPosition::CellCenter;
    auto* Transform = CastChecked<UPCGTransformPointsSettings>(ExactNode(*Graph,UPCGTransformPointsSettings::StaticClass())->GetSettings());
    Transform->OffsetMin = Transform->OffsetMax = FVector::ZeroVector;
    Transform->ScaleMin = Transform->ScaleMax = FVector(Profile.UniformScale);
    Transform->RotationMin = FRotator::ZeroRotator; Transform->RotationMax = FRotator(0,360,0);
    Transform->bApplyToAttribute = false; Transform->bAbsoluteOffset = false; Transform->bUniformScale = true;
    Transform->bAbsoluteScale = true; Transform->bAbsoluteRotation = false; Transform->bRecomputeSeed = false;
    auto* Filter = CastChecked<UPCGDensityFilterSettings>(ExactNode(*Graph,UPCGDensityFilterSettings::StaticClass())->GetSettings());
    Filter->LowerBound = Profile.Density == 0 ? 0 : 1; Filter->UpperBound = Filter->LowerBound; Filter->bInvertFilter = false;
    Filter->bNormalizeOutputDensity = false;
    auto* Spawner = CastChecked<UPCGStaticMeshSpawnerSettings>(ExactNode(*Graph,UPCGStaticMeshSpawnerSettings::StaticClass())->GetSettings());
    Spawner->bAllowDescriptorChanges = false;
    Spawner->bAllowMergeDifferentDataInSameInstancedComponents = false;
#if WITH_EDITORONLY_DATA
    Filter->bKeepZeroDensityPoints = false;
#endif
    Component.GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    Component.PostGenerateFunctionNames.Reset(); Component.bGenerateOnDropWhenTriggerOnDemand = false;
#if WITH_EDITOR
    Component.bRegenerateInEditor = false;
#endif
    Component.Seed = DerivedSeed; Component.SetGraphLocal(Graph);
    return FGamePlatformResult::Success();
}

FGamePlatformPCGOutputInspection GamePlatformPCGInspection::InspectOwnedOutput(UPCGComponent& Component,
    const UGamePlatformPCGProfileDefinition& Profile,const FVector& CenterCm)
{
    check(IsInGameThread()); FGamePlatformPCGOutputInspection Inspection;
    if (Component.IsGenerating() || Component.IsCleaningUp() || !Component.AreManagedResourcesAccessible() || Component.GetGeneratedGraphOutput().bCancelExecution)
    { Inspection.Result = Rejected(TEXT("NativeWorkNotDrained")); return Inspection; }
    // 即使允许零实例，也必须存在实际经过输出引脚的点数据；丢失输出不能冒充空区域。
    const auto& Output = Component.GetGeneratedGraphOutput();
    int64 OutputPointCount = 0;
    if (Output.TaggedData.Num() != 1)
    { Inspection.Result = Rejected(TEXT("MissingOrAmbiguousPointOutput")); return Inspection; }
    const auto* Points = Cast<UPCGBasePointData>(Output.TaggedData[0].Data);
    if (!Points || Output.TaggedData[0].Pin != PCGPinConstants::DefaultOutputLabel)
    { Inspection.Result = Rejected(TEXT("MissingPointOutput")); return Inspection; }
    OutputPointCount = Points->GetNumPoints();
    bool Valid = true; TArray<FString> CanonicalInstances;
    const FBox Bounds(CenterCm - Profile.HalfExtentCm,CenterCm + Profile.HalfExtentCm);
    TSet<const UInstancedStaticMeshComponent*> Inspected;
    Component.ForEachConstManagedResource([&](const UPCGManagedResource* Resource)
    {
        auto* Managed = Cast<UPCGManagedISMComponent>(Resource);
        auto* ISM = Managed ? Managed->GetComponent() : nullptr;
        if (!ISM || Resource->GetOuter() != &Component || !ISM->IsRegistered() || ISM->GetWorld() != Component.GetWorld() ||
            Inspected.Contains(ISM) || ISM->GetClass() != UInstancedStaticMeshComponent::StaticClass() || ISM->GetOwner() != Component.GetOwner() ||
            ISM->GetStaticMesh() != Profile.OutputMesh.Get() || ISM->GetIsReplicated() || ISM->GetOwner()->GetIsReplicated()) { Valid = false; return; }
        Inspected.Add(ISM);
        if (Profile.OutputUsage == EGamePlatformPCGOutputUsage::Cosmetic &&
            (ISM->GetCollisionEnabled() != ECollisionEnabled::NoCollision || ISM->GetGenerateOverlapEvents() || ISM->CanEverAffectNavigation())) { Valid = false; }
        if (Profile.OutputUsage == EGamePlatformPCGOutputUsage::StaticCollision && ISM->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics) { Valid = false; }
        Inspection.InstanceCount += ISM->GetInstanceCount();
        if (Inspection.InstanceCount > Profile.MaximumOutputs) { Valid = false; return; }
        for (int32 Index = 0; Index < ISM->GetInstanceCount(); ++Index)
        {
            FTransform Transform; if (!ISM->GetInstanceTransform(Index,Transform,true) || Transform.ContainsNaN() || !Bounds.IsInsideOrOn(Transform.GetLocation())) { Valid = false; continue; }
            const auto Position = Transform.GetLocation(); const auto Rotation = Transform.Rotator(); const auto Scale = Transform.GetScale3D();
            const auto Quantize = [](double Value,double Factor) { return FMath::RoundToInt64(Value * Factor); };
            CanonicalInstances.Add(FString::Printf(TEXT("%s|%lld,%lld,%lld|%lld,%lld,%lld|%lld,%lld,%lld"),*Profile.OutputMesh.ToSoftObjectPath().ToString(),
                Quantize(Position.X,10),Quantize(Position.Y,10),Quantize(Position.Z,10),Quantize(Rotation.Pitch,10000),Quantize(Rotation.Yaw,10000),Quantize(Rotation.Roll,10000),
                Quantize(Scale.X,10000),Quantize(Scale.Y,10000),Quantize(Scale.Z,10000)));
        }
    });
    Valid &= Inspection.InstanceCount == OutputPointCount && Inspection.InstanceCount >= Profile.MinimumOutputs && Inspection.InstanceCount <= Profile.MaximumOutputs;
    Valid &= Profile.Density == 0 ? Inspection.InstanceCount == 0 : Inspection.InstanceCount > 0;
    if (!Valid) { Inspection.Result = Rejected(TEXT("OutputAuditFailed")); return Inspection; }
    CanonicalInstances.Sort(); const FString Text = FString::Join(CanonicalInstances,TEXT("\n")); FTCHARToUTF8 Bytes(*Text);
    uint8 Digest[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.Get(),Bytes.Length(),Digest);
    Inspection.Fingerprint = TEXT("sha1-ism-mm-v1:") + BytesToHex(Digest,UE_ARRAY_COUNT(Digest));
    Inspection.Result = FGamePlatformResult::Success(); return Inspection;
}
