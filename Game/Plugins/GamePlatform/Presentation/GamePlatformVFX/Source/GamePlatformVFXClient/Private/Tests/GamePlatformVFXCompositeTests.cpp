#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXCompositeDefinitionDefaultsTest,
    "GamePlatform.VFX.Composite.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCompositeDefinitionDefaultsTest::RunTest(const FString& Parameters)
{
    const UGamePlatformVFXCompositeDefinition* Definition = NewObject<UGamePlatformVFXCompositeDefinition>();
    TestEqual(
        TEXT("Composite 行为类型正确"),
        Definition->GetBehavior(),
        EGamePlatformVFXBehavior::Composite);
    TestFalse(TEXT("Composite 默认不使用 Niagara 池"), Definition->AllowsPooling());
    TestTrue(TEXT("Composite 默认总生命周期必须大于0，避免无完成事件的父实例永久残留"), Definition->MaxTotalLifetimeSeconds > 0.0f);
    return true;
}

// F10：Steps的每一条边必须进入Data统一依赖图，否则租约预检无法发现A↔B环。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXCompositeDependencyTest,
    "GamePlatform.VFX.Composite.RequiredDependencyEdges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXCompositeDependencyTest::RunTest(const FString&)
{
    auto* Definition=NewObject<UGamePlatformVFXCompositeDefinition>();
    Definition->LogicalId.Namespace=TEXT("presentation.vfx"); Definition->LogicalId.Name=TEXT("parent"); Definition->LogicalId.LogicalVersion=1;
    Definition->DataVersion.SchemaVersion=1; Definition->DataVersion.ContentRevision=1;
    FGamePlatformVFXCompositeStep Step; Step.DefinitionId=TEXT("presentation.vfx.child@1");
    Definition->Steps.Add(Step);
    TestFalse(TEXT("没有依赖边的Steps必须在播放前失败"), Definition->ValidateDefinition().IsSuccess());
    Definition->RequiredDefinitions.Add(FPrimaryAssetId(TEXT("GamePlatformDefinition"), TEXT("presentation.vfx.child@1")));
    TestTrue(TEXT("完整声明依赖后允许Data继续递归验证"), Definition->ValidateDefinition().IsSuccess());
    return true;
}

#endif
