#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "NiagaraComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXInstanceLifecycleTest,
    "GamePlatform.VFX.Lifecycle.InstanceRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXInstanceLifecycleTest::RunTest(const FString& Parameters)
{
    FGamePlatformVFXInstanceRegistry Registry;
    const FGamePlatformVFXHandle Handle = Registry.Reserve();

    TestTrue(TEXT("Reserved handle 有效"), Handle.IsValid());
    TestTrue(TEXT("Pending 句柄视为活动"), Registry.IsActive(Handle));

    UNiagaraComponent* Component = NewObject<UNiagaraComponent>();
    TestTrue(
        TEXT("组件可以附着到实例Registry"),
        Registry.AttachComponent(Handle, Component, true));
    TestEqual(
        TEXT("Component反向索引O(1)返回原Handle"),
        Registry.FindByComponent(Component),
        Handle);

    TestTrue(TEXT("Stop 成功"), Registry.Stop(Handle, false));
    TestFalse(TEXT("Stop 后不再活动"), Registry.IsActive(Handle));
    TestFalse(
        TEXT("Stop 同步移除Component反向索引"),
        Registry.FindByComponent(Component).IsValid());
    return true;
}

#endif
