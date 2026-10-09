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
    // 测试只构造未附着 Actor 的临时 ASC，不应使用面向正式 Actor 生命周期的 AddSet。
    // 显式建立两个真实 AttributeSet 实例并登记到 ASC，才能准确测试 ViewModel 的订阅/读取。
    // 这不是生产初始化路径：正式角色仍由 CombatComponent 与 CharacterComponent 管理属性集。
    UGamePlatformAbilitySystemComponent* ASC = NewObject<UGamePlatformAbilitySystemComponent>();
    UGamePlatformCombatAttributeSet* Combat =
        NewObject<UGamePlatformCombatAttributeSet>(ASC);
    UDivineBeastsMomentumAttributeSet* Momentum =
        NewObject<UDivineBeastsMomentumAttributeSet>(ASC);
    ASC->AddAttributeSetSubobject(Combat);
    ASC->AddAttributeSetSubobject(Momentum);

    TestNotNull(TEXT("创建Combat AttributeSet"), Combat);
    TestNotNull(TEXT("创建Momentum AttributeSet"), Momentum);
    TestTrue(TEXT("ASC实际登记战斗属性集"),
        ASC->GetSet<UGamePlatformCombatAttributeSet>() == Combat);
    TestTrue(TEXT("ASC实际登记气势属性集"),
        ASC->GetSet<UDivineBeastsMomentumAttributeSet>() == Momentum);
    if (!Combat || !Momentum)
    {
        return false;
    }

    Combat->SetHealth(75.0f);
    Combat->SetMaxHealth(100.0f);
    Momentum->SetMomentum(40.0f);
    Momentum->SetMaxMomentum(100.0f);

    UDivineBeastsPlayerStatusViewModel* ViewModel = NewObject<UDivineBeastsPlayerStatusViewModel>();
    TestTrue(TEXT("ViewModel绑定ASC成功"), ViewModel->BindToAbilitySystem(ASC));
    const FDivineBeastsPlayerStatusViewData Status = ViewModel->GetStatus();
    TestEqual(TEXT("Health投影"), Status.Health, 75.0);
    TestEqual(TEXT("最大生命投影"), Status.MaxHealth, 100.0);
    TestEqual(TEXT("Momentum投影"), Status.Momentum, 40.0);
    TestFalse(TEXT("Health大于0时非死亡"), Status.bDead);

    ViewModel->UnbindFromAbilitySystem();
    return true;
}

#endif
