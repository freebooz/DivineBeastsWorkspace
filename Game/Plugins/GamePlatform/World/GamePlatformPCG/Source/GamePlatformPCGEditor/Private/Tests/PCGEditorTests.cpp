#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Manifests/PCGSourceFingerprint.h"
#include "Authoring/PCGDevelopmentGraph.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Services/GamePlatformPCGInspection.h"
#include "Engine/StaticMesh.h"

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
#endif
