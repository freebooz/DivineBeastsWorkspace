#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Characters/DivineBeastsGameplayCharacter.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerController.h"

/**
 * 验证新增业务类的真实反射继承和默认拒绝状态：
 * 无服务器身份/DefaultAbilitySet 的组件不得伪造授权，未配置技能定义的能力不可读数值。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAbilityAssemblyContractTest,
    "DivineBeasts.Abilities.Assembly.Contract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAbilityAssemblyContractTest::RunTest(const FString&)
{
    TestTrue(TEXT("正式项目角色继承ACharacter"),
        ADivineBeastsGameplayCharacter::StaticClass()->IsChildOf(ACharacter::StaticClass()));
    TestTrue(TEXT("技能角色继承真实权威Pawn生命周期"),
        ADivineBeastsGameplayCharacter::StaticClass()->IsChildOf(ADivineBeastsCharacter::StaticClass()));
    TestTrue(TEXT("项目技能类继承平台唯一GAS能力基类"),
        UDivineBeastsConfiguredGameplayAbility::StaticClass()->IsChildOf(
            UGamePlatformGameplayAbility::StaticClass()));

    // Actor/抽象 Ability 在UE测试中只读 CDO，禁止通过 NewObject 直接创建真实 Actor 或抽象类。
    const ADivineBeastsGameplayCharacter* Character =
        GetDefault<ADivineBeastsGameplayCharacter>();
    TestNotNull(TEXT("项目角色反射对象可创建"), Character);
    if (!Character)
    {
        return false;
    }
    TestNotNull(TEXT("角色携带唯一GAS能力组件"),
        Character->FindComponentByClass<UGamePlatformAbilitySystemComponent>());
    TestNotNull(TEXT("角色携带已有身份组件"),
        Character->FindComponentByClass<UDivineBeastsCharacterComponent>());
    TestNotNull(TEXT("角色携带平台战斗组件"),
        Character->FindComponentByClass<UGamePlatformCombatComponent>());

    UDivineBeastsAbilityLoadoutComponent* Loadout =
        Character->FindComponentByClass<UDivineBeastsAbilityLoadoutComponent>();
    TestNotNull(TEXT("角色携带技能授予状态组件"), Loadout);
    if (!Loadout)
    {
        return false;
    }
    TestFalse(TEXT("初始无真实授权时不得显示为已就绪"),
        Loadout->GetLoadoutState().bReady);
    TestEqual(TEXT("初始没有可释放或虚构的技能"), Loadout->GetLoadoutState().Slots.Num(), 0);
    // 继承合并必须保留每类唯一组件，不能只用FindComponent命中一份而掩盖重复ASC/权威状态。
    TArray<UGamePlatformAbilitySystemComponent*> AbilitySystems; Character->GetComponents(AbilitySystems);
    TArray<UGamePlatformCombatComponent*> Combats; Character->GetComponents(Combats);
    TArray<UDivineBeastsCharacterComponent*> Identities; Character->GetComponents(Identities);
    TArray<UGamePlatformGameplayEligibilityComponent*> Eligibilities; Character->GetComponents(Eligibilities);
    TArray<UDivineBeastsAbilityLoadoutComponent*> Loadouts; Character->GetComponents(Loadouts);
    TestEqual(TEXT("继承角色只有一份ASC"), AbilitySystems.Num(), 1);
    TestEqual(TEXT("继承角色只有一份Combat"), Combats.Num(), 1);
    TestEqual(TEXT("继承角色只有一份CharacterIdentity"), Identities.Num(), 1);
    TestEqual(TEXT("继承角色只有一份GameplayEligibility"), Eligibilities.Num(), 1);
    TestEqual(TEXT("继承角色只有一份AbilityLoadout"), Loadouts.Num(), 1);

    FDivineBeastsAbilityBalanceRow Row;
    FString Error;
    const UDivineBeastsConfiguredGameplayAbility* Ability =
        GetDefault<UDivineBeastsConfiguredGameplayAbility>();
    TestNotNull(TEXT("项目技能基类默认对象存在"), Ability);
    if (Ability)
    {
        TestFalse(TEXT("没有已加载主资产时拒绝读取伤害参数"),
            Ability->TryReadConfiguredBalance(Row, Error));
    }
    // 真正的拥有/失控事件必须沿权威基类清理。没有Definition的测试Pawn不能因新增Loadout而取得Active。
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("技能角色生命周期测试世界"), World)) { return false; }
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters SpawnParameters; SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pawn = World->SpawnActor<ADivineBeastsGameplayCharacter>(SpawnParameters);
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("真实技能Pawn"), Pawn) || !TestNotNull(TEXT("真实拥有Controller"), Controller))
    { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); return false; }
    Pawn->DispatchBeginPlay(); Controller->Possess(Pawn);
    auto* ASC = Pawn->GetGamePlatformAbilitySystemComponent();
    auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
    if (!TestNotNull(TEXT("活Pawn唯一ASC"), ASC) || !TestNotNull(TEXT("活Pawn权威资格组件"), Eligibility))
    { Pawn->Destroy(); World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); return false; }
    TestTrue(TEXT("拥有事件绑定继承的唯一ActorInfo"), ASC->GetAvatarActor() == Pawn);
    TestFalse(TEXT("缺Definition及准入不得Active"), Eligibility->IsServerPlayerActiveForGameplay());
    Controller->UnPossess();
    // 主分支OnRep_PlayerState入口也不能重新绑定失控的服务器Avatar。
    Pawn->OnRep_PlayerState();
    TestFalse(TEXT("失控及PlayerState通知后Avatar仍已撤销"), ASC->GetAvatarBindingSnapshot().bBound);
    TestFalse(TEXT("失控后资格仍失败关闭"), Eligibility->IsServerPlayerActiveForGameplay());
    Pawn->Destroy(); World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false);
    return true;
}
#endif
