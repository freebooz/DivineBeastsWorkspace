#if WITH_DEV_AUTOMATION_TESTS
// 真实GAS GameplayEffect测试：只核验一笔本地权威Effect的数值与限制，不冒充有目标命中的双客户端联机。
#include "Misc/AutomationTest.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Effects/DivineBeastsMomentumGainGameplayEffect.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMomentumGameplayEffectTest,
    "DivineBeasts.Characters.Momentum.RealGameplayEffect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMomentumGameplayEffectTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("真实独立World"), World)) return false;
    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("真实Actor"), Owner))
    {
        World->DestroyWorld(false);
        return false;
    }
    UGamePlatformAbilitySystemComponent* ASC =
        NewObject<UGamePlatformAbilitySystemComponent>(Owner);
    Owner->AddInstanceComponent(ASC);
    ASC->RegisterComponent();
    ASC->InitAbilityActorInfo(Owner, Owner);
    // 气势GAS属性集由Actor拥有并登记到ASC，避免以ASC为Outer导致引擎用例失败。
    UDivineBeastsMomentumAttributeSet* Momentum =
        NewObject<UDivineBeastsMomentumAttributeSet>(Owner);
    ASC->AddAttributeSetSubobject(Momentum);
    if (!TestNotNull(TEXT("GAS注册气势属性集"), Momentum))
    {
        Owner->Destroy(); World->DestroyWorld(false);
        return false;
    }

    const UDivineBeastsMomentumGainGameplayEffect* EffectCDO =
        GetDefault<UDivineBeastsMomentumGainGameplayEffect>();
    TestEqual(TEXT("气势奖励效果只包含一个修改器"), EffectCDO->Modifiers.Num(), 1);
    TestTrue(TEXT("气势奖励必须是InstantGE"),
        EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant);
    if (EffectCDO->Modifiers.Num() == 1)
    {
        TestTrue(TEXT("只能修改项目气势属性"),
            EffectCDO->Modifiers[0].Attribute ==
                UDivineBeastsMomentumAttributeSet::GetMomentumAttribute());
        TestTrue(TEXT("仅进行加法"),
            EffectCDO->Modifiers[0].ModifierOp == EGameplayModOp::Additive);
        TestEqual(TEXT("只接受稳定SetByCaller身份"),
            EffectCDO->Modifiers[0].ModifierMagnitude.GetSetByCallerFloat().DataName,
            UDivineBeastsMomentumGainGameplayEffect::GetMagnitudeDataName());
    }

    const auto ApplyGain = [ASC](float Gain)
    {
        FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
            UDivineBeastsMomentumGainGameplayEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
        if (!Spec.IsValid() || !Spec.Data.IsValid()) return false;
        Spec.Data->SetSetByCallerMagnitude(
            UDivineBeastsMomentumGainGameplayEffect::GetMagnitudeDataName(), Gain);
        return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
    };
    TestTrue(TEXT("真实服务端GE应用10点"), ApplyGain(10.0f));
    TestEqual(TEXT("GAS气势获得并反映在AttributeSet"), Momentum->GetMomentum(), 10.0f);
    TestTrue(TEXT("真实GE应用120点，由GAS限制在100"), ApplyGain(120.0f));
    TestEqual(TEXT("气势不可超过100"), Momentum->GetMomentum(), 100.0f);
    Owner->Destroy();
    World->DestroyWorld(false);
    return true;
}
#endif
