#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXHandleValidityTest,
    "GamePlatform.VFX.Handle.Validity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXHandleValidityTest::RunTest(const FString& Parameters)
{
    FGamePlatformVFXHandle Handle;
    TestFalse(TEXT("默认 Handle 无效"), Handle.IsValid());

    Handle.Id = FGuid::NewGuid();
    Handle.Generation = 1;
    TestTrue(TEXT("完整 Handle 有效"), Handle.IsValid());

    FGamePlatformVFXHandle OldGeneration = Handle;
    OldGeneration.Generation = 0;
    TestFalse(TEXT("零 Generation 无效"), OldGeneration.IsValid());
    UWorld* WorldA = NewObject<UWorld>();
    UWorld* WorldB = NewObject<UWorld>();
    Handle.World = WorldA;
    TestTrue(TEXT("Handle识别所属World"), Handle.BelongsToWorld(WorldA));
    TestFalse(TEXT("Handle拒绝跨World身份"), Handle.BelongsToWorld(WorldB));

    FGamePlatformVFXHandle OtherWorld = Handle;
    OtherWorld.World = WorldB;
    TestFalse(TEXT("相同Id/Generation但不同World不是同一句柄"), OtherWorld == Handle);
    return true;
}

#endif
