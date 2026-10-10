#include "Authoring/PCGDevelopmentGraph.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGProjectionElement.h"
#include "Elements/PCGSplineSampler.h"
#include "PCGInputOutputSettings.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Nodes/GamePlatformPCGNodes.h"
#include "Services/GamePlatformPCGTemplateContract.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"

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
    // P1/Scatter/Biome/InterfaceBand与简单Assembly有明确的自采样入口：
    // 使用UE原生网格点生成器，避免Graph Input为空时仅转换已有点而永远得不到候选。
    // 农田、围栏、连接件仍消费上游地块/样条候选，不虚构空间几何。
    if (TemplateId == FGamePlatformPCGTemplateIds::ScatterSurface ||
        TemplateId == FGamePlatformPCGTemplateIds::BiomeGenerator ||
        TemplateId == FGamePlatformPCGTemplateIds::InterfaceBand ||
        TemplateId == FGamePlatformPCGTemplateIds::AssemblySpawn)
    {
        UPCGCreatePointsGridSettings* Grid = nullptr;
        UPCGNode* GridNode = Graph->AddNodeOfType(Grid);
        if (!GridNode || !Grid)
        {
            Error = TEXT("Foundation曲面散布模板无法创建官方CreatePointsGrid采样器。");
            return nullptr;
        }
        Grid->GridExtents = FVector(500.0, 500.0, 0.0);
        Grid->CellSize = FVector(150.0, 150.0, 100.0);
        Grid->CoordinateSpace = EPCGCoordinateSpace::OriginalComponent;
        Grid->PointPosition = EPCGPointPosition::CellCenter;
        Grid->bCullPointsOutsideVolume = false;
        Grid->SetExecuteOnGPU(false);
        Tail = GridNode;
        TailPin = PCGPinConstants::DefaultOutputLabel;
    }
    // 线性/地块原语必须先使用官方SplineSampler把样条几何变为真正PCG点，再交给协议和策略节点。
    // 连接件无样条时继续消费调用方明确提供的点数据，禁止在平台层假设项目桥/门布局。
    if (TemplateId == FGamePlatformPCGTemplateIds::LinearDresser ||
        TemplateId == FGamePlatformPCGTemplateIds::Enclosure ||
        TemplateId == FGamePlatformPCGTemplateIds::EnclosureClosed ||
        TemplateId == FGamePlatformPCGTemplateIds::ParcelFill ||
        TemplateId == FGamePlatformPCGTemplateIds::CropField)
    {
        UPCGSplineSamplerSettings* Sampler = nullptr;
        UPCGNode* SamplerNode = Graph->AddNodeOfType(Sampler);
        if (!SamplerNode || !Sampler)
        {
            Error = TEXT("Foundation线性/地块模板无法创建官方SplineSampler。");
            return nullptr;
        }
        const bool bInterior = TemplateId == FGamePlatformPCGTemplateIds::ParcelFill ||
            TemplateId == FGamePlatformPCGTemplateIds::CropField;
        Sampler->SamplerParams.Dimension = bInterior ?
            EPCGSplineSamplingDimension::OnInterior : EPCGSplineSamplingDimension::OnSpline;
        Sampler->SamplerParams.Mode = EPCGSplineSamplingMode::Distance;
        Sampler->SamplerParams.DistanceIncrement = 200.0f;
        Sampler->SamplerParams.bUnbounded = false;
        Sampler->SetExecuteOnGPU(false);
        if (!Graph->AddEdge(Tail, TailPin, SamplerNode, PCGPinConstants::DefaultInputLabel))
        {
            Error = TEXT("Foundation模板Spline数据与官方Sampler输入接线失败。");
            return nullptr;
        }
        Tail = SamplerNode;
        TailPin = PCGPinConstants::DefaultOutputLabel;
    }

    if (!AppendTemplateNode<UGamePlatformPCGWriteSchemaDefaultsSettings>(*Graph, Tail, TailPin, Error))
    {
        return nullptr;
    }
    // 只在有空间消费意义的模板中插入通用数值几何排除。
    // 真实Mask由WorldDirector在编辑器阶段收集并注入图实例；不在PCG执行线程扫描世界Actor。
    if (TemplateId == FGamePlatformPCGTemplateIds::ScatterSurface ||
        TemplateId == FGamePlatformPCGTemplateIds::BiomeGenerator ||
        TemplateId == FGamePlatformPCGTemplateIds::InterfaceBand ||
        TemplateId == FGamePlatformPCGTemplateIds::CropField ||
        TemplateId == FGamePlatformPCGTemplateIds::AssemblySpawn)
    {
        if (!AppendTemplateNode<UGamePlatformPCGSpatialCarveSettings>(*Graph, Tail, TailPin, Error))
        {
            return nullptr;
        }
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

UPCGGraph* GamePlatformPCGEditor::CreateFoundationRealizedGraph(
    UObject* Outer, FName Name, const UGamePlatformPCGProfileDefinition& Profile,
    const UGamePlatformPCGMeshSetDefinition& MeshSet, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();

    // 项目内容只传入已从GamePlatformData获得租约的定义与网格，不在编辑器生成器内同步加载资源。
    // 道路/围栏的按跨度多MeshSet选择需另行配置；当前不得用单一MeshSet伪装这一逻辑已完成。
    const bool bSupported = Profile.TemplateId == FGamePlatformPCGTemplateIds::ScatterSurface ||
        Profile.TemplateId == FGamePlatformPCGTemplateIds::BiomeGenerator ||
        Profile.TemplateId == FGamePlatformPCGTemplateIds::InterfaceBand ||
        Profile.TemplateId == FGamePlatformPCGTemplateIds::CropField ||
        Profile.TemplateId == FGamePlatformPCGTemplateIds::AssemblySpawn;
    const FPrimaryAssetId ActualId = MeshSet.GetPrimaryAssetId();
    if (!Outer || Name.IsNone() || !bSupported || Profile.ExecutionPolicy != EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic ||
        !ActualId.IsValid() || Profile.MeshSetDefinitionId != ActualId ||
        !Profile.RequiredDefinitions.Contains(ActualId) ||
        !MeshSet.ValidateDefinition().IsSuccess() || !Profile.ValidateDefinition().IsSuccess())
    {
        Error = TEXT("真实图需要批准模板、有效Profile、已声明并校验的MeshSet Definition与编辑器静态策略。");
        return nullptr;
    }

    const bool bCollision = Profile.OutputUsage == EGamePlatformPCGOutputUsage::StaticCollision;
    if (Profile.OutputUsage != EGamePlatformPCGOutputUsage::Cosmetic && !bCollision)
    {
        Error = TEXT("真实图输出用途未获批准。");
        return nullptr;
    }
    for (const FGamePlatformPCGMeshSetEntry& Item : MeshSet.Entries)
    {
        // 只接受已加载并由调用者租约维持生命周期的网格，拒绝偷偷同步LoadSynchronous。
        if (!Item.Mesh.Get() || !FMath::IsFinite(Item.Weight) || Item.Weight <= 0.0f ||
            Item.Weight > 1000.0f || Item.bStaticCollision != bCollision)
        {
            Error = TEXT("MeshSet网格尚未取得有效租约、权重不合法或碰撞用途不一致；禁止生成半成品。");
            return nullptr;
        }
    }

    UPCGGraph* Graph = CreateFoundationTemplateGraph(Outer, Name, Profile.TemplateId, Error);
    if (!Graph)
    {
        return nullptr;
    }

    UPCGNode* Validator = nullptr;
    int32 MeshSetterCount = 0;
    for (UPCGNode* Node : Graph->GetNodes())
    {
        if (!Node || !Node->GetSettings()) { continue; }
        if (Node->GetSettings()->IsA<UGamePlatformPCGValidateSchemaSettings>())
        {
            if (Validator) { Error = TEXT("SchemaValidator不能重复。"); return nullptr; }
            Validator = Node;
        }
        if (auto* Assign = Cast<UGamePlatformPCGAssignMeshSetSettings>(Node->GetSettings()))
        {
            Assign->MeshSetId = FName(*ActualId.ToString());
            ++MeshSetterCount;
        }
    }

    const TArray<FPCGPinProperties> GraphOutputs = Graph->DefaultOutputPinProperties();
    if (!Validator || MeshSetterCount != 1 || !Graph->GetOutputNode() || GraphOutputs.IsEmpty() ||
        !Graph->RemoveEdge(Validator, PCGPinConstants::DefaultOutputLabel,
            Graph->GetOutputNode(), GraphOutputs[0].Label))
    {
        Error = TEXT("真实图缺少唯一MeshSet赋值/Schema校验节点，或不能安全断开原始输出边。");
        return nullptr;
    }

    UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
    UPCGNode* SpawnerNode = Graph->AddNodeOfType(Spawner);
    if (!SpawnerNode || !Spawner)
    {
        Error = TEXT("无法创建UE5.8官方StaticMeshSpawner节点。");
        return nullptr;
    }
    Spawner->SetExecuteOnGPU(false);
    Spawner->bDebug = false;
    Spawner->bEnabled = true;
    Spawner->bAllowDescriptorChanges = false;
    Spawner->bAllowMergeDifferentDataInSameInstancedComponents = false;
    Spawner->OutAttributeName = TEXT("Mesh");
    Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    UPCGMeshSelectorWeighted* Selector = Cast<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
    if (!Selector)
    {
        Error = TEXT("官方WeightedMeshSelector类型与UE5.8接口不匹配。");
        return nullptr;
    }

    Selector->MeshEntries.Reset();
    for (const FGamePlatformPCGMeshSetEntry& Item : MeshSet.Entries)
    {
        FPCGMeshSelectorWeightedEntry& Entry = Selector->MeshEntries.AddDefaulted_GetRef();
        Entry.Weight = FMath::Clamp(FMath::RoundToInt(Item.Weight * 100.0f), 1, 100000);
        Entry.Descriptor.StaticMesh = Item.Mesh.Get();
        Entry.Descriptor.ComponentClass = UInstancedStaticMeshComponent::StaticClass();
        Entry.Descriptor.Mobility = EComponentMobility::Static;
        Entry.Descriptor.bUseDefaultCollision = false;
        Entry.Descriptor.bGenerateOverlapEvents = false;
        Entry.Descriptor.bCanEverAffectNavigation = bCollision;
        Entry.Descriptor.BodyInstance.SetCollisionProfileName(
            bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        Entry.Descriptor.BodyInstance.SetCollisionEnabled(
            bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    if (!Graph->AddEdge(Validator, PCGPinConstants::DefaultOutputLabel,
            SpawnerNode, PCGPinConstants::DefaultInputLabel) ||
        !Graph->AddEdge(SpawnerNode, PCGPinConstants::DefaultOutputLabel,
            Graph->GetOutputNode(), GraphOutputs[0].Label))
    {
        Error = TEXT("真实网格生成器与Schema校验/图输出连接失败。");
        return nullptr;
    }

    // Foundation仍为纯逻辑模板。项目图实例不是模板，不回写平台共享Graph。
    Graph->bIsTemplate = false;
    Graph->bExposeToLibrary = false;
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
