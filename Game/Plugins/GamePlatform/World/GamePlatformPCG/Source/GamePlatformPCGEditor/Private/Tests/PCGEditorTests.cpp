#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Manifests/PCGSourceFingerprint.h"
#include "Authoring/PCGDevelopmentGraph.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "PCGNode.h"
#include "Services/GamePlatformPCGInspection.h"
#include "Services/GamePlatformPCGTemplateContract.h"
#include "Engine/StaticMesh.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "Elements/PCGProjectionElement.h"
#include "Elements/PCGSplineSampler.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Nodes/GamePlatformPCGNodes.h"

// 顺序不参与来源身份；依赖内容和引擎版本必须参与，不能只哈希路径或随机种子。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGSourceFingerprintTest, "GamePlatform.PCG.Editor.SourceFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGSourceFingerprintTest::RunTest(const FString& Parameters)
{
    const FString Baseline = GamePlatformPCGEditor::HashRecords({TEXT("mesh|bytes-a"), TEXT("engine|5.8.0")});
    TestEqual(TEXT("依赖顺序不改变指纹"), Baseline,
        GamePlatformPCGEditor::HashRecords({TEXT("engine|5.8.0"), TEXT("mesh|bytes-a")}));
    TestNotEqual(TEXT("真实依赖字节改变使清单失效"), Baseline,
        GamePlatformPCGEditor::HashRecords({TEXT("engine|5.8.0"), TEXT("mesh|bytes-b")}));
    TestNotEqual(TEXT("引擎版本参与指纹"), Baseline,
        GamePlatformPCGEditor::HashRecords({TEXT("engine|5.8.1"), TEXT("mesh|bytes-a")}));
    return true;
}

// 真实UObject图制作测试，但不执行生成；不能代替PCG任务、保存重开或碰撞测试。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGDevelopmentGraphTest, "GamePlatform.PCG.Editor.DevelopmentGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGDevelopmentGraphTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("真实引擎基础网格"), Mesh)) { return false; }
    for (bool bStaticCollision : {false, true})
    {
        auto* Profile = NewObject<UGamePlatformPCGProfileDefinition>();
        FGamePlatformId::TryParse(TEXT("test.pcg_editor@1"), Profile->LogicalId);
        FGamePlatformId::TryParse(TEXT("test.region@1"), Profile->RegionId);
        Profile->ExecutionPolicy = EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic;
        Profile->OutputUsage = bStaticCollision ? EGamePlatformPCGOutputUsage::StaticCollision : EGamePlatformPCGOutputUsage::Cosmetic;
        Profile->OutputMesh = Mesh;
        FString Error;
        UPCGGraph* Graph = GamePlatformPCGEditor::CreateDevelopmentGraph(Profile, NAME_None, Mesh, bStaticCollision, Error);
        if (!TestNotNull(*Error, Graph)) { return false; }
        Profile->GraphReference = Graph;
        TestEqual(TEXT("恰好四个原生处理节点"), Graph->GetNodes().Num(), 4);
        TestTrue(TEXT("完整图通过runtime公开检查"), GamePlatformPCGInspection::ValidateApprovedGraph(*Profile).IsSuccess());
    }
    return true;
}

// M0/M1 Foundation模板只在内存创建并验证合同，不保存资产；真实落盘由显式Editor工具入口执行。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGFoundationTemplateGraphTest, "GamePlatform.PCG.Editor.FoundationTemplateContracts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGFoundationTemplateGraphTest::RunTest(const FString& Parameters)
{
    const TArray<FName> TemplateIds =
    {
        FGamePlatformPCGTemplateIds::Base,
        FGamePlatformPCGTemplateIds::ScatterSurface,
        FGamePlatformPCGTemplateIds::BiomeGenerator,
        FGamePlatformPCGTemplateIds::LinearDresser,
        FGamePlatformPCGTemplateIds::Enclosure,
        FGamePlatformPCGTemplateIds::EnclosureClosed,
        FGamePlatformPCGTemplateIds::Connector,
        FGamePlatformPCGTemplateIds::GateInsert,
        FGamePlatformPCGTemplateIds::ParcelFill,
        FGamePlatformPCGTemplateIds::CropField,
        FGamePlatformPCGTemplateIds::AssemblySpawn,
        FGamePlatformPCGTemplateIds::InterfaceBand
    };

    for (const FName TemplateId : TemplateIds)
    {
        auto* Profile = NewObject<UGamePlatformPCGProfileDefinition>();
        FGamePlatformId::TryParse(TEXT("test.pcg_foundation_template@1"), Profile->LogicalId);
        FGamePlatformId::TryParse(TEXT("test.region@1"), Profile->RegionId);
        Profile->ExecutionPolicy = EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic;
        Profile->OutputUsage = EGamePlatformPCGOutputUsage::Cosmetic;
        Profile->TemplateId = TemplateId;
        Profile->TemplateVersion = 1;
        // Foundation Template（基础模板）是平台逻辑模板，不绑定具体项目网格。
        // 真实网格通过项目 Graph Instance（图实例）+ MeshSet Definition（网格集合定义）在后续阶段注入。
        TestTrue(*FString::Printf(TEXT("%s模板不要求绑定OutputMesh"), *TemplateId.ToString()), Profile->OutputMesh.IsNull());

        FString Error;
        UPCGGraph* Graph = GamePlatformPCGEditor::CreateFoundationTemplateGraph(Profile, TemplateId, TemplateId, Error);
        if (!TestNotNull(*FString::Printf(TEXT("%s模板图创建：%s"), *TemplateId.ToString(), *Error), Graph))
        {
            return false;
        }

        Profile->GraphReference = Graph;
        TestTrue(*FString::Printf(TEXT("%s标记为PCG模板"), *TemplateId.ToString()), Graph->bIsTemplate);
        TestTrue(*FString::Printf(TEXT("%s通过Template Contract"), *TemplateId.ToString()),
            GamePlatformPCGInspection::ValidateApprovedGraph(*Profile).IsSuccess());
        // P1自己产生Scatter/Assembly点；P2道路/围栏/地块必须用官方SplineSampler而非空点透传。
        if (TemplateId == FGamePlatformPCGTemplateIds::ScatterSurface ||
            TemplateId == FGamePlatformPCGTemplateIds::BiomeGenerator ||
            TemplateId == FGamePlatformPCGTemplateIds::InterfaceBand ||
            TemplateId == FGamePlatformPCGTemplateIds::AssemblySpawn)
        {
            TestTrue(TEXT("散布模板含实际原生点采样器"), Graph->GetNodes().ContainsByPredicate(
                [](const UPCGNode* Node)
                {
                    return Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGCreatePointsGridSettings>();
                }));
        }
        if (TemplateId == FGamePlatformPCGTemplateIds::LinearDresser ||
            TemplateId == FGamePlatformPCGTemplateIds::Enclosure ||
            TemplateId == FGamePlatformPCGTemplateIds::EnclosureClosed ||
            TemplateId == FGamePlatformPCGTemplateIds::ParcelFill ||
            TemplateId == FGamePlatformPCGTemplateIds::CropField)
        {
            TestTrue(TEXT("线性/地块模板含官方SplineSampler"), Graph->GetNodes().ContainsByPredicate(
                [](const UPCGNode* Node)
                {
                    return Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGSplineSamplerSettings>();
                }));
        }
    }

    return true;
}

// 七个M0/M1 Foundation Subgraph（基础公共子图）必须能在内存中真实构造；不保存资产。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGFoundationSubgraphTest, "GamePlatform.PCG.Editor.FoundationSubgraphContracts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGFoundationSubgraphTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (const FName SubgraphId : FGamePlatformPCGSubgraphIds::All())
    {
        UObject* Outer = NewObject<UObject>(GetTransientPackage());
        FString Error;
        UPCGGraph* Graph = GamePlatformPCGEditor::CreateFoundationSubgraphGraph(Outer, SubgraphId, SubgraphId, Error);
        if (!TestNotNull(*FString::Printf(TEXT("%s子图创建：%s"), *SubgraphId.ToString(), *Error), Graph))
        {
            return false;
        }
        TestTrue(*FString::Printf(TEXT("%s公开为可复用PCG模板/子图"), *SubgraphId.ToString()), Graph->bIsTemplate && Graph->bExposeToLibrary);

        if (SubgraphId == FGamePlatformPCGSubgraphIds::ProjectOnLandscape)
        {
            const UPCGGraphInputOutputSettings* InputSettings = Cast<UPCGGraphInputOutputSettings>(Graph->GetInputNode()->GetSettings());
            TestNotNull(TEXT("ProjectOnLandscape存在Graph Input设置"), InputSettings);
            if (InputSettings)
            {
                const TArray<FPCGPinProperties> OutputPins = InputSettings->DefaultOutputPinProperties();
                TestTrue(TEXT("ProjectOnLandscape公开Landscape输入Pin"), OutputPins.ContainsByPredicate(
                    [](const FPCGPinProperties& Pin) { return Pin.Label == PCGInputOutputConstants::DefaultLandscapeLabel; }));
            }
            TestTrue(TEXT("ProjectOnLandscape使用官方Projection节点"), Graph->GetNodes().ContainsByPredicate(
                [](const UPCGNode* Node) { return Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGProjectionSettings>(); }));
        }
        else if (SubgraphId == FGamePlatformPCGSubgraphIds::WriteClosedExclude)
        {
            TestTrue(TEXT("WriteClosedExclude使用统一WriteExclude节点"), Graph->GetNodes().ContainsByPredicate(
                [](const UPCGNode* Node) { return Node && Node->GetSettings() && Node->GetSettings()->IsA<UGamePlatformPCGWriteExcludeSettings>(); }));
        }
    }
    return true;
}

// 真实UE5.8 Editor图制作合同：独立项目Graph Instance应包含官方Spawner并完全复用数据定义。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPCGFoundationRealizedGraphTest,
    "GamePlatform.PCG.Editor.RealizedGraphMeshSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGFoundationRealizedGraphTest::RunTest(const FString&)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("真实引擎基础网格可用"), Mesh)) { return false; }

    UGamePlatformPCGMeshSetDefinition* MeshSet = NewObject<UGamePlatformPCGMeshSetDefinition>();
    FGamePlatformId::TryParse(TEXT("test.pcg_mesh_set@1"), MeshSet->LogicalId);
    FGamePlatformPCGMeshSetEntry& MeshEntry = MeshSet->Entries.AddDefaulted_GetRef();
    MeshEntry.Mesh = Mesh;
    MeshEntry.Weight = 1.0f;
    MeshEntry.bStaticCollision = false;

    UGamePlatformPCGProfileDefinition* Profile = NewObject<UGamePlatformPCGProfileDefinition>();
    FGamePlatformId::TryParse(TEXT("test.pcg_realization@1"), Profile->LogicalId);
    FGamePlatformId::TryParse(TEXT("test.region@1"), Profile->RegionId);
    Profile->TemplateId = FGamePlatformPCGTemplateIds::ScatterSurface;
    Profile->TemplateVersion = 1;
    Profile->ExecutionPolicy = EGamePlatformPCGExecutionPolicy::EditorGeneratedStatic;
    Profile->OutputUsage = EGamePlatformPCGOutputUsage::Cosmetic;
    Profile->MeshSetDefinitionId = MeshSet->GetPrimaryAssetId();
    Profile->RequiredDefinitions.Add(Profile->MeshSetDefinitionId);

    FString Error;
    UPCGGraph* Template = GamePlatformPCGEditor::CreateFoundationTemplateGraph(
        Profile, TEXT("PCGTemplateGraph"), Profile->TemplateId, Error);
    if (!TestNotNull(*Error, Template)) { return false; }
    Profile->GraphReference = Template;

    UPCGGraph* Realized = GamePlatformPCGEditor::CreateFoundationRealizedGraph(
        Profile, TEXT("PCGRealizedGraph"), *Profile, *MeshSet, Error);
    if (!TestNotNull(*Error, Realized)) { return false; }

    TestFalse(TEXT("项目绑定图不再是平台模板"), Realized->bIsTemplate);
    const bool bHasSpawner = Realized->GetNodes().ContainsByPredicate([](const UPCGNode* Node)
    {
        return Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGStaticMeshSpawnerSettings>();
    });
    TestTrue(TEXT("实际绑定官方StaticMeshSpawner"), bHasSpawner);
    Profile->GraphReference = Realized;
    TestTrue(TEXT("完整真实图通过平台Template Contract"), GamePlatformPCGInspection::ValidateApprovedGraph(*Profile).IsSuccess());

    Profile->RequiredDefinitions.Remove(Profile->MeshSetDefinitionId);
    TestFalse(TEXT("没有通过GamePlatformData登记MeshSet时必须失败"), Profile->ValidateDefinition().IsSuccess());
    return true;
}

#endif
