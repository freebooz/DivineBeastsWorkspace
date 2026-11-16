// 真实GAS与引擎Timer回归；隔离测试世界不冒充正式地图准入或人工玩法验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Abilities/DivineBeastsDevelopmentMagicAbility.h"
#include "Settings/GamePlatformCombatSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "TimerManager.h"

namespace
{
struct FResourceFixture
{
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<AActor> Actor;
    TStrongObjectPtr<UGamePlatformAbilitySystemComponent> ASC;
    TStrongObjectPtr<UGamePlatformCombatComponent> Combat;
    const UGamePlatformCombatAttributeSet* Attributes = nullptr;
    bool Initialize()
    {
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values));
        if(!World.IsValid())return false;
        World->InitializeActorsForPlay(FURL());
        Actor.Reset(World->SpawnActor<AActor>());
        ASC.Reset(NewObject<UGamePlatformAbilitySystemComponent>(Actor.Get()));
        Combat.Reset(NewObject<UGamePlatformCombatComponent>(Actor.Get()));
        Actor->AddInstanceComponent(ASC.Get());Actor->AddInstanceComponent(Combat.Get());
        ASC->RegisterComponent();Combat->RegisterComponent();ASC->InitAbilityActorInfo(Actor.Get(),Actor.Get());
        Actor->DispatchBeginPlay();Attributes=Combat->GetCombatAttributeSet();
        // 由真实宿主后续帧推进此隔离TimerManager，不修改全局GFrameCounter。
        World->GetTimerManager().Tick(0.f);
        return Attributes!=nullptr;
    }
    ~FResourceFixture(){if(Actor.IsValid())Actor->Destroy();if(World.IsValid())World->DestroyWorld(false);}
};

struct FResourceRecoveryCommand final : IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TArray<TSharedPtr<FResourceFixture>> Fixtures;
    int32 Steps=0;
    FResourceRecoveryCommand(FAutomationTestBase* InTest,TArray<TSharedPtr<FResourceFixture>> InFixtures)
        :Test(InTest),Fixtures(MoveTemp(InFixtures)){}
    bool Update() override
    {
        for(const auto& F:Fixtures)F->World->GetTimerManager().Tick(.25f);
        if(++Steps<4)return false;
        Test->TestEqual(TEXT("真实1秒Timer恢复5点：75到80"),Fixtures[0]->Attributes->GetEnergy(),80.f);
        Test->TestEqual(TEXT("恢复封顶100"),Fixtures[1]->Attributes->GetEnergy(),100.f);
        Test->TestEqual(TEXT("Owner销毁后不再恢复"),Fixtures[2]->Attributes->GetEnergy(),75.f);
        Test->TestEqual(TEXT("生命为0时不能恢复能量"),Fixtures[3]->Attributes->GetEnergy(),75.f);
        Fixtures.Reset();return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefaultCombatResourceTest,"DivineBeasts.Server.DefaultCombatResources",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FDefaultCombatResourceTest::RunTest(const FString&)
{
    const auto* Settings=GetDefault<UGamePlatformCombatSettings>();
    TestEqual(TEXT("SSLM配置恢复每秒5"),Settings->EnergyRegenerationPerSecond,5.f);
    TestEqual(TEXT("SSLM配置法术默认25"),Settings->DefaultAbilityEnergyCost,25.f);
    TArray<TSharedPtr<FResourceFixture>> F;
    for(int32 I=0;I<4;++I){auto Item=MakeShared<FResourceFixture>();if(!TestTrue(TEXT("真实权威世界/ASC/Combat"),Item->Initialize()))return false;F.Add(Item);}
    TestEqual(TEXT("出生生命100"),F[0]->Attributes->GetHealth(),100.f);
    TestEqual(TEXT("出生能量100"),F[0]->Attributes->GetEnergy(),100.f);
    TStrongObjectPtr<UDivineBeastsDevelopmentMagicAbility> Spell(NewObject<UDivineBeastsDevelopmentMagicAbility>(F[0]->ASC.Get()));
    UGameplayEffect* Cost=Spell->GetCostGameplayEffect();if(!TestNotNull(TEXT("原生法术成本GE"),Cost))return false;
    TestTrue(TEXT("100能量可通过GAS成本资格"),F[0]->ASC->CanApplyAttributeModifiers(Cost,1.f,F[0]->ASC->MakeEffectContext()));
    F[0]->ASC->ApplyGameplayEffectToSelf(Cost,1.f,F[0]->ASC->MakeEffectContext());
    TestEqual(TEXT("一次成本扣25：100到75"),F[0]->Attributes->GetEnergy(),75.f);
    F[0]->ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetEnergyAttribute(),24.f);
    TestFalse(TEXT("不足25被GAS拒绝"),F[0]->ASC->CanApplyAttributeModifiers(Cost,1.f,F[0]->ASC->MakeEffectContext()));
    TestEqual(TEXT("资格检查不扣能量"),F[0]->Attributes->GetEnergy(),24.f);
    F[0]->ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetEnergyAttribute(),75.f);
    F[1]->ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetEnergyAttribute(),98.f);
    F[2]->ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetEnergyAttribute(),75.f);F[2]->Actor->Destroy();
    F[3]->ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetHealthAttribute(),0.f);
    F[3]->ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetEnergyAttribute(),75.f);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FResourceRecoveryCommand>(this,MoveTemp(F)));
    return true;
}
#endif
