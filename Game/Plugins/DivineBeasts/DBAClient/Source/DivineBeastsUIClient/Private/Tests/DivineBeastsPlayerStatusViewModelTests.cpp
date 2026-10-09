#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "ViewModels/Combat/DivineBeastsPlayerStatusViewModel.h"
#include "Characters/DivineBeastsGameplayCharacter.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsPlayerStatusViewModelSnapshotTest,
    "DivineBeasts.UI.Combat.PlayerStatusSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsPlayerStatusViewModelSnapshotTest::RunTest(const FString&)
{
    // GAS AttributeSet必须直接归Actor所有，ASC也须具有真实Owner/Avatar；旧夹具归ASC会在SetHealth时Fatal。
    // 使用独立瞬态世界内的现有原生角色，无后端、无BeginPlay；正式初始化仍由角色组件负责。
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("测试世界创建成功"), World)) return false;
    auto* Character = World->SpawnActor<ADivineBeastsGameplayCharacter>();
    auto* ASC = Character ? Cast<UGamePlatformAbilitySystemComponent>(Character->GetAbilitySystemComponent()) : nullptr;
    if (!TestNotNull(TEXT("真实角色持有ASC"), ASC)) { World->DestroyWorld(false); return false; }
    ASC->InitAbilityActorInfo(Character, Character);
    UGamePlatformCombatAttributeSet* Combat =
        NewObject<UGamePlatformCombatAttributeSet>(Character);
    UDivineBeastsMomentumAttributeSet* Momentum =
        NewObject<UDivineBeastsMomentumAttributeSet>(Character);
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
        World->DestroyWorld(false);
        return false;
    }

    Combat->SetMaxHealth(100.0f);
    Combat->SetHealth(75.0f);
    Momentum->SetMaxMomentum(100.0f);
    Momentum->SetMomentum(40.0f);

    UDivineBeastsPlayerStatusViewModel* ViewModel = NewObject<UDivineBeastsPlayerStatusViewModel>();
    TestTrue(TEXT("ViewModel绑定ASC成功"), ViewModel->BindToAbilitySystem(ASC));
    const FDivineBeastsPlayerStatusViewData Status = ViewModel->GetStatus();
    TestEqual(TEXT("Health投影"), Status.Health, 75.0);
    TestEqual(TEXT("最大生命投影"), Status.MaxHealth, 100.0);
    TestEqual(TEXT("Momentum投影"), Status.Momentum, 40.0);
    TestFalse(TEXT("Health大于0时非死亡"), Status.bDead);

    ViewModel->UnbindFromAbilitySystem();
    World->DestroyWorld(false);
    return true;
}

#endif
