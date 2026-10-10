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
#include "Nodes/GamePlatformPCGNodes.h"
#include "Services/GamePlatformPCGTemplateContract.h"

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

/** 沿PCG边的上游→下游方向检查节点可达性，避免模板仅“摆着”Schema节点却不在真实执行链上。 */
bool IsNodeReachable(const UPCGNode& Start,const UPCGNode& Target)
{
    TArray<const UPCGNode*> Pending;
    TSet<const UPCGNode*> Visited;
    Pending.Add(&Start);

    while (!Pending.IsEmpty())
    {
        const UPCGNode* Current = Pending.Pop(EAllowShrinking::No);
        if (!Current || Visited.Contains(Current))
        {
            continue;
        }
        if (Current == &Target)
        {
            return true;
        }
        Visited.Add(Current);

        for (const UPCGPin* Pin : Current->GetOutputPins())
        {
            if (!Pin) { continue; }
            for (const UPCGEdge* Edge : Pin->Edges)
            {
                if (!Edge || Edge->InputPin != Pin || !Edge->OutputPin || !Edge->OutputPin->Node)
                {
                    continue;
                }
                Pending.Add(Edge->OutputPin->Node);
            }
        }

        // 模板最大节点数已限制为64；额外保护异常循环/损坏图，避免校验无界扩张。
        if (Visited.Num() > 128)
        {
            return false;
        }
    }

    return false;
}

bool ValidateSpawnerDescriptor(const UPCGStaticMeshSpawnerSettings& Spawner, const UGamePlatformPCGProfileDefinition& Profile)
{
    const UPCGMeshSelectorWeighted* Selector = Cast<UPCGMeshSelectorWeighted>(Spawner.MeshSelectorParameters);
    if (!Selector || Selector->GetClass() != UPCGMeshSelectorWeighted::StaticClass() ||
        Selector->MeshEntries.IsEmpty() || Selector->MeshEntries.Num() > 64 ||
        Selector->bUseAttributeMaterialOverrides || !Spawner.PostProcessFunctionNames.IsEmpty() || !Spawner.TargetActor.IsNull() ||
        !Spawner.StaticMeshComponentPropertyOverrides.IsEmpty() || Spawner.InstanceDataPackerParameters || Spawner.InstanceDataPackerType ||
        Spawner.MeshSelectorType != UPCGMeshSelectorWeighted::StaticClass() || !Selector->MaterialOverrideAttributes.IsEmpty() ||
        Spawner.bAllowDescriptorChanges || Spawner.bAllowMergeDifferentDataInSameInstancedComponents)
    {
        return false;
    }

    const bool bLegacy = Profile.TemplateId.IsNone();
    if (bLegacy && Selector->MeshEntries.Num() != 1) { return false; }
    if (!bLegacy && !Profile.MeshSetDefinitionId.IsValid()) { return false; }

    for (const FPCGMeshSelectorWeightedEntry& Entry : Selector->MeshEntries)
    {
        const auto& Descriptor = Entry.Descriptor;
        if (Descriptor.StaticMesh.IsNull() || !Descriptor.OverrideMaterials.IsEmpty() ||
            Descriptor.ComponentClass != UInstancedStaticMeshComponent::StaticClass() ||
            Entry.Weight <= 0 || Descriptor.bUseDefaultCollision || Descriptor.bGenerateOverlapEvents ||
            (bLegacy && Descriptor.StaticMesh.ToSoftObjectPath() != Profile.OutputMesh.ToSoftObjectPath()))
        {
            return false;
        }
        if (Profile.OutputUsage == EGamePlatformPCGOutputUsage::Cosmetic &&
            (Descriptor.BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
             Descriptor.bCanEverAffectNavigation))
        {
            return false;
        }
        if (Profile.OutputUsage == EGamePlatformPCGOutputUsage::StaticCollision &&
            Descriptor.BodyInstance.GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
        {
            return false;
        }
    }
    return true;
}

FGamePlatformResult ValidateTemplateGraph(const UGamePlatformPCGProfileDefinition& Profile, UPCGGraph& Graph)
{
    if (!FGamePlatformPCGTemplateContract::IsTemplateHeaderValid(Profile))
    {
        return Rejected(TEXT("InvalidTemplateContract"));
    }

    // HiGen/GPU继续失败关闭；M2需要同时升级执行、缓存、分区和资产门禁后才能开放。
    if (Graph.IsHierarchicalGenerationEnabled() || Graph.GetNodes().IsEmpty() || Graph.GetNodes().Num() > 64)
    {
        return Rejected(TEXT("UnsupportedTemplateShape"));
    }

    UPCGNode* SchemaWriterNode = nullptr;
    UPCGNode* SchemaValidatorNode = nullptr;
    UPCGNode* SpawnerNode = nullptr;

    for (UPCGNode* Node : Graph.GetNodes())
    {
        const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
        if (!Settings || !Settings->bEnabled || Settings->ShouldExecuteOnGPU() ||
            Node->GetSettingsInterface() != Settings ||
            !FGamePlatformPCGTemplateContract::IsApprovedSettingsClass(Settings->GetClass()))
        {
            return Rejected(TEXT("UnapprovedTemplateNode"));
        }

        if (Settings->IsA<UGamePlatformPCGWriteSchemaDefaultsSettings>())
        {
            if (SchemaWriterNode) { return Rejected(TEXT("DuplicateTemplateSchemaWriter")); }
            SchemaWriterNode = Node;
        }
        if (Settings->IsA<UGamePlatformPCGValidateSchemaSettings>())
        {
            if (SchemaValidatorNode) { return Rejected(TEXT("DuplicateTemplateSchemaValidator")); }
            SchemaValidatorNode = Node;
        }

        if (const UPCGStaticMeshSpawnerSettings* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings))
        {
            if (SpawnerNode || Graph.bIsTemplate || !ValidateSpawnerDescriptor(*Spawner, Profile))
            {
                return Rejected(TEXT("TemplateSpawnerSideEffectsForbidden"));
            }
            SpawnerNode = Node;
        }
    }

    UPCGNode* OutputNode = Graph.GetOutputNode();
    if (!SchemaWriterNode || !SchemaValidatorNode || !OutputNode)
    {
        return Rejected(TEXT("TemplateSchemaBoundaryMissing"));
    }
    if (!IsNodeReachable(*SchemaWriterNode,*SchemaValidatorNode) || !IsNodeReachable(*SchemaValidatorNode,*OutputNode))
    {
        return Rejected(TEXT("TemplateSchemaBoundaryDisconnected"));
    }
    // Foundation模板只能输出通过Schema校验的点；绑定项目MeshSet后，必须在校验后真正生成网格。
    // 运行服务仍失败关闭，避免把编辑器静态图意外用于客户端权威/专用服务器。
    if (!Graph.bIsTemplate && (!SpawnerNode || !Profile.MeshSetDefinitionId.IsValid() ||
        !IsNodeReachable(*SchemaValidatorNode,*SpawnerNode) || !IsNodeReachable(*SpawnerNode,*OutputNode)))
    {
        return Rejected(TEXT("RealizedTemplateSpawnerMissing"));
    }

    return FGamePlatformResult::Success();
}
}

FGamePlatformResult GamePlatformPCGInspection::ValidateApprovedGraph(const UGamePlatformPCGProfileDefinition& Profile)
{
    check(IsInGameThread()); const auto Valid = Profile.ValidateDefinition(); if (!Valid.IsSuccess()) { return Valid; }
    auto* Graph = Profile.GraphReference.Get(); auto* Mesh = Profile.OutputMesh.Get();
    if (!Graph || (Profile.TemplateId.IsNone() && !Mesh)) { return Rejected(TEXT("ProfileAssetsNotLeased")); }
    // 1.0模板走合同式白名单；TemplateId为空时继续使用0.1.0固定四节点夹具，保证渐进迁移与可回退。
    if (!Profile.TemplateId.IsNone())
    {
        return ValidateTemplateGraph(Profile, *Graph);
    }
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
    const auto* Settings = CastChecked<UPCGStaticMeshSpawnerSettings>(Spawner->GetSettings());
    if (!ValidateSpawnerDescriptor(*Settings, Profile))
    { return Rejected(TEXT("SpawnerSideEffectsForbidden")); }
    return FGamePlatformResult::Success();
}

FGamePlatformResult GamePlatformPCGInspection::ConfigureOwnedComponent(UPCGComponent& Component,
    const UGamePlatformPCGProfileDefinition& Profile,int32 DerivedSeed)
{
    check(IsInGameThread()); const auto Valid = ValidateApprovedGraph(Profile); if (!Valid.IsSuccess()) { return Valid; }
    if (Component.IsGenerating() || Component.IsCleaningUp() || Component.IsPartitioned()) { return Rejected(TEXT("ComponentBusyOrPartitioned")); }
    // 1.0 Template Contract（模板合同）当前只完成Editor校验与源码节点基础。
    // 生产模板参数绑定、MeshSet目录解析和静态Bake工作流尚未闭环，因此运行服务必须失败关闭，
    // 不能继续按0.1.0固定四节点结构CastChecked，否则合法的新模板图会触发断言。
    if (!Profile.TemplateId.IsNone())
    {
        return FGamePlatformResult::Unsupported(
            TEXT("PCGTemplateRuntimeExecutionDeferred"),
            TEXT("1.0模板图当前仅支持合同校验；运行时执行需等待真实模板资产、参数绑定和输出审查闭环。"));
    }
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
