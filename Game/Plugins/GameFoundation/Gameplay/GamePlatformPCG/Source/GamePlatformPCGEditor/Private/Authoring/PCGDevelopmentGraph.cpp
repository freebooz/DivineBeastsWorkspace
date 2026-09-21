#include "Authoring/PCGDevelopmentGraph.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"

UPCGGraph* GamePlatformPCGEditor::CreateDevelopmentGraph(UObject* Outer, FName Name,
    UStaticMesh* Mesh, bool bStaticCollision, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    if (!Outer || !Mesh)
    {
        Error = TEXT("缺少图所有者或真实网格资源");
        return nullptr;
    }
    UPCGGraph* Graph = NewObject<UPCGGraph>(Outer, Name, RF_Public | RF_Standalone | RF_Transactional);
    UPCGCreatePointsGridSettings* Grid = nullptr;
    UPCGTransformPointsSettings* Transform = nullptr;
    UPCGDensityFilterSettings* Filter = nullptr;
    UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
    UPCGNode* GridNode = Graph->AddNodeOfType(Grid);
    UPCGNode* TransformNode = Graph->AddNodeOfType(Transform);
    UPCGNode* FilterNode = Graph->AddNodeOfType(Filter);
    UPCGNode* SpawnerNode = Graph->AddNodeOfType(Spawner);
    if (!GridNode || !TransformNode || !FilterNode || !SpawnerNode || !Graph->GetOutputNode())
    {
        Error = TEXT("原生PCG节点创建失败；未保存或执行图");
        return nullptr;
    }
    Grid->GridExtents = FVector(500, 500, 0);
    Grid->CellSize = FVector(100, 100, 100);
    Grid->CoordinateSpace = EPCGCoordinateSpace::OriginalComponent;
    Grid->PointPosition = EPCGPointPosition::CellCenter;
    Grid->PointSteepness = 1.0f;
    Grid->bCullPointsOutsideVolume = false;
    Transform->OffsetMin = Transform->OffsetMax = FVector::ZeroVector;
    Transform->RotationMin = FRotator::ZeroRotator;
    Transform->RotationMax = FRotator(0, 360, 0);
    Transform->ScaleMin = Transform->ScaleMax = FVector::OneVector;
    Transform->bAbsoluteScale = true;
    Transform->bUniformScale = true;
    Transform->bApplyToAttribute = false;
    Transform->bRecomputeSeed = false;
    Transform->SetExecuteOnGPU(false);
    Filter->LowerBound = Filter->UpperBound = 1.0f;
    Filter->bInvertFilter = false;
    Filter->bNormalizeOutputDensity = false;
    Filter->bKeepZeroDensityPoints = false;
    Spawner->SetExecuteOnGPU(false);
    Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    auto* Selector = Cast<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
    if (!Selector)
    {
        Error = TEXT("原生Weighted选择器创建失败");
        return nullptr;
    }
    Selector->MeshEntries.Reset();
    FPCGMeshSelectorWeightedEntry& Entry = Selector->MeshEntries.AddDefaulted_GetRef();
    Entry.Weight = 1;
    Entry.Descriptor.StaticMesh = Mesh;
    Entry.Descriptor.ComponentClass = UInstancedStaticMeshComponent::StaticClass();
    Entry.Descriptor.Mobility = EComponentMobility::Static;
    Entry.Descriptor.bUseDefaultCollision = false;
    Entry.Descriptor.bGenerateOverlapEvents = false;
    Entry.Descriptor.bCanEverAffectNavigation = false;
    Entry.Descriptor.BodyInstance.SetCollisionProfileName(bStaticCollision
        ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
    Entry.Descriptor.BodyInstance.SetCollisionEnabled(bStaticCollision
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Spawner->bAllowDescriptorChanges = false;
    Spawner->bAllowMergeDifferentDataInSameInstancedComponents = false;
    Spawner->OutAttributeName = TEXT("Mesh");
    for (UPCGNode* Node : Graph->GetNodes())
    {
        Node->GetSettings()->bDebug = false;
        Node->GetSettings()->bEnabled = true;
    }
    const FName OutPin = PCGPinConstants::DefaultOutputLabel;
    const FName InPin = PCGPinConstants::DefaultInputLabel;
    if (!Graph->AddEdge(GridNode, OutPin, TransformNode, InPin) ||
        !Graph->AddEdge(TransformNode, OutPin, FilterNode, InPin) ||
        !Graph->AddEdge(FilterNode, OutPin, SpawnerNode, InPin) ||
        !Graph->AddEdge(SpawnerNode, OutPin, Graph->GetOutputNode(), OutPin))
    {
        Error = TEXT("原生PCG四节点接线失败；未保存或执行图");
        return nullptr;
    }
    return Graph;
}
