#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "ViewModels/Combat/DivineBeastsPlayerStatusViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsPlayerStatusViewModelSnapshotTest,
    "DivineBeasts.UI.Combat.PlayerStatusSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsPlayerStatusViewModelSnapshotTest::RunTest(const FString&)
{
    UGamePlatformAbilitySystemComponent* ASC = NewObject<UGamePlatformAbilitySystemComponent>();
    UGamePlatformCombatAttributeSet* Combat = const_cast<UGamePlatformCombatAttributeSet*>(
        ASC->AddSet<UGamePlatformCombatAttributeSet>());
    UDivineBeastsMomentumAttributeSet* Momentum = const_cast<UDivineBeastsMomentumAttributeSet*>(
        ASC->AddSet<UDivineBeastsMomentumAttributeSet>());

    TestNotNull(TEXT("创建Combat AttributeSet"), Combat);
    TestNotNull(TEXT("创建Momentum AttributeSet"), Momentum);
    if (!Combat || !Momentum)
    {
        return false;
    }

    Combat->SetHealth(75.0f);
    Combat->SetMaxHealth(100.0f);
    Combat->SetShield(20.0f);
    Combat->SetMaxShield(50.0f);
    Momentum->SetMomentum(40.0f);
    Momentum->SetMaxMomentum(100.0f);

    UDivineBeastsPlayerStatusViewModel* ViewModel = NewObject<UDivineBeastsPlayerStatusViewModel>();
    TestTrue(TEXT("ViewModel绑定ASC成功"), ViewModel->BindToAbilitySystem(ASC));
    const FDivineBeastsPlayerStatusViewData Status = ViewModel->GetStatus();
    TestEqual(TEXT("Health投影"), Status.Health, 75.0);
    TestEqual(TEXT("Shield投影"), Status.Shield, 20.0);
    TestEqual(TEXT("Momentum投影"), Status.Momentum, 40.0);
    TestFalse(TEXT("Health大于0时非死亡"), Status.bDead);

    ViewModel->UnbindFromAbilitySystem();
    return true;
}

#endif
