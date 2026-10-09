#if WITH_DEV_AUTOMATION_TESTS
// 真正Editor非PIE世界不能创建AI运行注册服务；该测试必须由锁定UE自动化执行。
#include "Misc/AutomationTest.h"
#include "Perception/GamePlatformAITargetRegistrySubsystem.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAITargetRegistryEditorWorldTest,
    "GamePlatform.AI.Lifecycle.EditorWorldExcluded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAITargetRegistryEditorWorldTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
    TestNull(TEXT("编辑地图不能创建会Spawn/Possess的AI服务"), World->GetSubsystem<UGamePlatformAITargetRegistrySubsystem>());
    World->DestroyWorld(false); return true;
}
#endif
