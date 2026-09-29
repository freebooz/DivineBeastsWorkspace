#include "Authoring/PCGDevelopmentGraph.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGProjectionElement.h"
#include "PCGInputOutputSettings.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Nodes/GamePlatformPCGNodes.h"
#include "Services/GamePlatformPCGTemplateContract.h"

namespace
{
template <typename TSettings>
UPCGNode* AddTemplateNode(UPCGGraph& Graph, FString& Error)
{
    TSettings* Settings = nullptr;
    UPCGNode* Node = Graph.AddNodeOfType(Settings);
    if (!Node || !Settings)
    {
        Error = FString::Printf(TEXT("Foundation模板节点创建失败：%s"), *TSettings::StaticClass()->GetName());
        return nullptr;
    }

    Settings->bDebug = false;
    Settings->bEnabled = true;
    Settings->SetExecuteOnGPU(false);
    return Node;
}

template <typename TSettings>
bool AppendTemplateNode(UPCGGraph& Graph, UPCGNode*& Tail, FName& TailPin, FString& Error)
{
    UPCGNode* Node = AddTemplateNode<TSettings>(Graph, Error);
    if (!Node)
    {
        return false;
    }

    if (!Graph.AddEdge(Tail, TailPin, Node, PCGPinConstants::DefaultInputLabel))
    {
        Error = FString::Printf(TEXT("Foundation模板节点接线失败：%s"), *TSettings::StaticClass()->GetName());
        return false;
    }

    Tail = Node;
    TailPin = PCGPinConstants::DefaultOutputLabel;
    return true;
}

bool IsM0M1Template(FName TemplateId)
{
    using namespace GamePlatformPCGEditor;
    return TemplateId == FGamePlatformPCGTemplateIds::Base ||
           TemplateId == FGamePlatformPCGTemplateIds::ScatterSurface ||
           TemplateId == FGamePlatformPCGTemplateIds::BiomeGenerator ||
           TemplateId == FGamePlatformPCGTemplateIds::LinearDresser ||
           TemplateId == FGamePlatformPCGTemplateIds::Enclosure ||
           TemplateId == FGamePlatformPCGTemplateIds::EnclosureClosed ||
           TemplateId == FGamePlatformPCGTemplateIds::Connector ||
           TemplateId == FGamePlatformPCGTemplateIds::GateInsert ||
           TemplateId == FGamePlatformPCGTemplateIds::ParcelFill ||
           TemplateId == FGamePlatformPCGTemplateIds::CropField ||
           TemplateId == FGamePlatformPCGTemplateIds::AssemblySpawn ||
           TemplateId == FGamePlatformPCGTemplateIds::InterfaceBand;
}

bool IsM0M1Subgraph(FName SubgraphId)
{
    return FGamePlatformPCGSubgraphIds::IsKnown(SubgraphId);
}

bool ConnectTailToOutput(UPCGGraph& Graph, UPCGNode* Tail, FName TailPin, FString& Error)
{
    const TArray<FPCGPinProperties> Outputs = Graph.DefaultOutputPinProperties();
    if (!Tail || Outputs.IsEmpty() || !Graph.GetOutputNode() ||
        !Graph.AddEdge(Tail, TailPin, Graph.GetOutputNode(), Outputs[0].Label))
    {
        Error = TEXT("Foundation子图连接到Graph Output失败。");
        return false;
    }
    return true;
}
}

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

UPCGGraph* GamePlatformPCGEditor::CreateFoundationTemplateGraph(
    UObject* Outer,
    FName Name,
    FName TemplateId,
    FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    if (!Outer || Name.IsNone() || !IsM0M1Template(TemplateId))
    {
        Error = TEXT("只允许为有效Outer创建已批准的M0/M1 Foundation模板；M2+模板继续后置。");
        return nullptr;
    }

    UPCGGraph* Graph = NewObject<UPCGGraph>(Outer, Name, RF_Public | RF_Standalone | RF_Transactional);
    if (!Graph || !Graph->GetInputNode() || !Graph->GetOutputNode())
    {
        Error = TEXT("Foundation模板UPCGGraph或默认输入/输出节点创建失败。");
        return nullptr;
    }

    Graph->bIsTemplate = true;
    Graph->bExposeToLibrary = true;

    const TArray<FPCGPinProperties> GraphInputs = Graph->DefaultInputPinProperties();
    const TArray<FPCGPinProperties> GraphOutputs = Graph->DefaultOutputPinProperties();
    if (GraphInputs.IsEmpty() || GraphOutputs.IsEmpty())
    {
        Error = TEXT("Foundation模板缺少官方PCG默认输入或输出引脚。");
        return nullptr;
    }

    UPCGNode* Tail = Graph->GetInputNode();
    FName TailPin = GraphInputs[0].Label;
    if (!AppendTemplateNode<UGamePlatformPCGWriteSchemaDefaultsSettings>(*Graph, Tail, TailPin, Error))
    {
        return nullptr;
    }

    if (TemplateId == FGamePlatformPCGTemplateIds::ScatterSurface ||
        TemplateId == FGamePlatformPCGTemplateIds::BiomeGenerator ||
        TemplateId == FGamePlatformPCGTemplateIds::InterfaceBand)
    {
        if (!AppendTemplateNode<UGamePlatformPCGProjectAlignSettings>(*Graph, Tail, TailPin, Error) ||
            !AppendTemplateNode<UGamePlatformPCGApplySpawnPolicySettings>(*Graph, Tail, TailPin, Error) ||
            !AppendTemplateNode<UGamePlatformPCGAssignMeshSetSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
    }
    else if (TemplateId == FGamePlatformPCGTemplateIds::LinearDresser ||
             TemplateId == FGamePlatformPCGTemplateIds::Enclosure ||
             TemplateId == FGamePlatformPCGTemplateIds::EnclosureClosed)
    {
        if (!AppendTemplateNode<UGamePlatformPCGFitPostsToSplineSettings>(*Graph, Tail, TailPin, Error) ||
            !AppendTemplateNode<UGamePlatformPCGSelectSpanMeshByLengthSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
    }
    else if (TemplateId == FGamePlatformPCGTemplateIds::Connector ||
             TemplateId == FGamePlatformPCGTemplateIds::GateInsert)
    {
        if (!AppendTemplateNode<UGamePlatformPCGBreakSpansByTagsSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
    }
    else if (TemplateId == FGamePlatformPCGTemplateIds::ParcelFill ||
             TemplateId == FGamePlatformPCGTemplateIds::CropField)
    {
        if (!AppendTemplateNode<UGamePlatformPCGBuildRowsSettings>(*Graph, Tail, TailPin, Error) ||
            !AppendTemplateNode<UGamePlatformPCGAssignMeshSetSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
    }
    else if (TemplateId == FGamePlatformPCGTemplateIds::AssemblySpawn)
    {
        if (!AppendTemplateNode<UGamePlatformPCGAssignMeshSetSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
    }

    if (!AppendTemplateNode<UGamePlatformPCGValidateSchemaSettings>(*Graph, Tail, TailPin, Error))
    {
        return nullptr;
    }

    if (!Graph->AddEdge(Tail, TailPin, Graph->GetOutputNode(), GraphOutputs[0].Label))
    {
        Error = TEXT("Foundation模板ValidateSchema到Graph Output接线失败。");
        return nullptr;
    }

    return Graph;
}

UPCGGraph* GamePlatformPCGEditor::CreateFoundationSubgraphGraph(
    UObject* Outer,
    FName Name,
    FName SubgraphId,
    FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    if (!Outer || Name.IsNone() || !IsM0M1Subgraph(SubgraphId))
    {
        Error = TEXT("只允许创建已登记的M0/M1 Foundation Subgraph（基础子图）。");
        return nullptr;
    }

    UPCGGraph* Graph = NewObject<UPCGGraph>(Outer, Name, RF_Public | RF_Standalone | RF_Transactional);
    if (!Graph || !Graph->GetInputNode() || !Graph->GetOutputNode())
    {
        Error = TEXT("Foundation子图UPCGGraph或默认输入/输出节点创建失败。");
        return nullptr;
    }

    Graph->bIsTemplate = true;
    Graph->bExposeToLibrary = true;
    const TArray<FPCGPinProperties> Inputs = Graph->DefaultInputPinProperties();
    if (Inputs.IsEmpty())
    {
        Error = TEXT("Foundation子图缺少官方默认输入引脚。");
        return nullptr;
    }

    UPCGNode* Tail = Graph->GetInputNode();
    FName TailPin = Inputs[0].Label;

    if (SubgraphId == FGamePlatformPCGSubgraphIds::ProjectOnLandscape)
    {
        UPCGGraphInputOutputSettings* InputSettings = Cast<UPCGGraphInputOutputSettings>(Graph->GetInputNode()->GetSettings());
        if (!InputSettings)
        {
            Error = TEXT("SG_ProjectOnLandscape无法取得Graph Input设置。");
            return nullptr;
        }
        const FPCGPinProperties& LandscapePin = InputSettings->AddPin(
            FPCGPinProperties(PCGInputOutputConstants::DefaultLandscapeLabel, EPCGDataType::Landscape));

        UPCGProjectionSettings* ProjectionSettings = nullptr;
        UPCGNode* ProjectionNode = Graph->AddNodeOfType(ProjectionSettings);
        if (!ProjectionNode || !ProjectionSettings)
        {
            Error = TEXT("SG_ProjectOnLandscape创建官方Projection节点失败。");
            return nullptr;
        }
        ProjectionSettings->bForceCollapseToPoint = true;
        ProjectionSettings->SetExecuteOnGPU(false);
        if (!Graph->AddEdge(Tail, TailPin, ProjectionNode, PCGPinConstants::DefaultInputLabel) ||
            !Graph->AddEdge(Graph->GetInputNode(), LandscapePin.Label, ProjectionNode, PCGProjectionConstants::ProjectionTargetLabel))
        {
            Error = TEXT("SG_ProjectOnLandscape投影输入接线失败。");
            return nullptr;
        }
        Tail = ProjectionNode;
        TailPin = PCGPinConstants::DefaultOutputLabel;
        if (!AppendTemplateNode<UGamePlatformPCGProjectAlignSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
    }
    else if (SubgraphId == FGamePlatformPCGSubgraphIds::PriorityCarve)
    {
        if (!AppendTemplateNode<UGamePlatformPCGPriorityCarveSettings>(*Graph, Tail, TailPin, Error)) { return nullptr; }
    }
    else if (SubgraphId == FGamePlatformPCGSubgraphIds::ApplySpawnPolicy)
    {
        if (!AppendTemplateNode<UGamePlatformPCGApplySpawnPolicySettings>(*Graph, Tail, TailPin, Error)) { return nullptr; }
    }
    else if (SubgraphId == FGamePlatformPCGSubgraphIds::AssignMeshSet)
    {
        if (!AppendTemplateNode<UGamePlatformPCGAssignMeshSetSettings>(*Graph, Tail, TailPin, Error)) { return nullptr; }
    }
    else if (SubgraphId == FGamePlatformPCGSubgraphIds::FitPostsToSpline)
    {
        if (!AppendTemplateNode<UGamePlatformPCGFitPostsToSplineSettings>(*Graph, Tail, TailPin, Error)) { return nullptr; }
    }
    else if (SubgraphId == FGamePlatformPCGSubgraphIds::BreakByIntersection)
    {
        if (!AppendTemplateNode<UGamePlatformPCGBreakSpansByTagsSettings>(*Graph, Tail, TailPin, Error)) { return nullptr; }
        if (auto* Settings = Cast<UGamePlatformPCGBreakSpansByTagsSettings>(Tail->GetSettings()))
        {
            Settings->BlockingTags = {TEXT("Road"), TEXT("Gate"), TEXT("Exclusion")};
        }
    }
    else if (SubgraphId == FGamePlatformPCGSubgraphIds::WriteClosedExclude)
    {
        if (!AppendTemplateNode<UGamePlatformPCGWriteExcludeSettings>(*Graph, Tail, TailPin, Error)) { return nullptr; }
        if (auto* Settings = Cast<UGamePlatformPCGWriteExcludeSettings>(Tail->GetSettings()))
        {
            Settings->ExcludeSource = TEXT("ClosedEnclosure");
            Settings->ExcludeStrength = 1.0f;
        }
    }

    return ConnectTailToOutput(*Graph, Tail, TailPin, Error) ? Graph : nullptr;
}
