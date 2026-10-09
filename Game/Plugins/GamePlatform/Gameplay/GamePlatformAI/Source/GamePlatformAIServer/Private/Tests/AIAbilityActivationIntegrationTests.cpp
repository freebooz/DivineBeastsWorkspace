#if WITH_DEV_AUTOMATION_TESTS
// 真实AIController→Data资源完成→ASC中立Gate→TryActivateAbilitiesByTag→平台CanActivateAbility回归。
// 必须提供真实开发夹具定义与具体能力资产；无夹具明确失败，不创建生产假Definition/假租约/固定成功Gate。
#include "Misc/AutomationTest.h"
#include "Controllers/GamePlatformAIController.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Abilities/GamePlatformGameplayAbility.h"
#include "Data/GamePlatformAIDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"
namespace
{
class FAIAbilityIntegrationCommand final : public IAutomationLatentCommand
{
public:
    FAIAbilityIntegrationCommand(FAutomationTestBase& InTest, FString InDefinitionPath, FString InAbilityClassPath)
        : Test(InTest), DefinitionPath(MoveTemp(InDefinitionPath)), AbilityClassPath(MoveTemp(InAbilityClassPath)),
          StartedSeconds(FPlatformTime::Seconds()) {}
    ~FAIAbilityIntegrationCommand() override { Cleanup(); }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        { Test.AddError(TEXT("AI资源/Brain/Tag激活集成30秒未完成，不视为通过。")); Cleanup(); return true; }
        if (Phase == 0)
        {
            // 测试前置夹具允许同步读具体能力类；生产AI定义与Brain仍走真实Data异步租约。
            UClass* AbilityClass = FSoftClassPath(AbilityClassPath).TryLoadClass<UGamePlatformGameplayAbility>();
            if (!AbilityClass || AbilityClass->HasAnyClassFlags(CLASS_Abstract))
            { Test.AddError(TEXT("必须提供具体、可激活的平台能力开发夹具类。")); return true; }
            const auto* Ability = AbilityClass->GetDefaultObject<UGamePlatformGameplayAbility>();
            Tags = Ability->GetAssetTags();
            if (Tags.IsEmpty()) { Test.AddError(TEXT("能力开发夹具必须登记非空Tag，不能用空Tag掩盖攻击选择。")); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*(TEXT("AIAbilityTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))));
            World.Reset(UWorld::CreateWorld(EWorldType::Game, false)); World->SetGameInstance(Instance.Get());
            World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
                .CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false));
            Pawn = World->SpawnActor<APawn>();
            auto* State = NewObject<UGamePlatformAIStateComponent>(Pawn); Pawn->AddInstanceComponent(State); State->RegisterComponent();
            State->InitializeServerState(TEXT("Test.AI.Pending"), 1);
            State->SetDefinitionAsset(TSoftObjectPtr<UGamePlatformAIDefinition>(FSoftObjectPath(DefinitionPath)));
            ASC = NewObject<UGamePlatformAbilitySystemComponent>(Pawn); Pawn->AddInstanceComponent(ASC); ASC->RegisterComponent();
            Controller = World->SpawnActor<AGamePlatformAIController>(); Controller->Possess(Pawn);
            SpecHandle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
            Test.TestFalse(TEXT("资源/Brain完成前Tag实际入口拒绝"), ASC->TryActivateAbilitiesByTag(Tags, false));
            Phase = 1; return false;
        }
        if (Phase == 1)
        {
            const auto Error = Controller->GetLastAIError();
            // 无目标/无导航属于后续决策结果，不能误判为Brain资源初始化失败。
            if (Error == EGamePlatformAIError::InvalidDefinition || Error == EGamePlatformAIError::MissingStateComponent ||
                Error == EGamePlatformAIError::UnsupportedBrain || Error == EGamePlatformAIError::AssetLoadFailed ||
                Error == EGamePlatformAIError::NotAuthority || Error == EGamePlatformAIError::WorldTearingDown)
            { Test.AddError(TEXT("真实AI定义/Brain初始化失败，检查夹具类型、BT/BB和Data配置。")); Cleanup(); return true; }
            if (!ASC->EvaluateActivationEligibility().IsSuccess()) { return false; }
            auto* Definition = Cast<UGamePlatformAIDefinition>(FSoftObjectPath(DefinitionPath).ResolveObject());
            if (!Definition || !Tags.HasTagExact(Definition->PrimaryAbilityTag))
            { Test.AddError(TEXT("真实定义攻击Tag必须精确存在于能力夹具。")); Cleanup(); return true; }
            FGameplayTagContainer AttackTags; AttackTags.AddTag(Definition->PrimaryAbilityTag);
            Test.TestTrue(TEXT("真实就绪AI中立Gate允许Tag链经过平台CanActivateAbility"), ASC->TryActivateAbilitiesByTag(AttackTags, false));
            Controller->UnPossess();
            Test.TestFalse(TEXT("失去Pawn撤销Gate/ActorInfo，Tag入口拒绝"), ASC->TryActivateAbilitiesByTag(AttackTags, false));
            ASC->BindAbilityActorInfo(Pawn, Pawn);
            Test.TestFalse(TEXT("Controller委托已解绑，重新绑定不能复活旧Gate"), ASC->EvaluateActivationEligibility().IsSuccess());
            Cleanup(); return true;
        }
        return true;
    }
private:
    void Cleanup()
    {
        if (ASC && SpecHandle.IsValid()) { ASC->ClearAbility(SpecHandle); SpecHandle = {}; }
        if (World.IsValid()) { World->DestroyWorld(false); World.Reset(); }
        ASC = nullptr; Controller = nullptr; Pawn = nullptr;
        if (Instance.IsValid())
        {
            UWorld* InstanceWorld = Instance->GetWorld();
            if (InstanceWorld) { InstanceWorld->DestroyWorld(false); }
            Instance->Shutdown();
            if (InstanceWorld) { GEngine->DestroyWorldContext(InstanceWorld); }
            Instance.Reset();
        }
    }
    FAutomationTestBase& Test;
    FString DefinitionPath, AbilityClassPath;
    double StartedSeconds = 0; int32 Phase = 0;
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UWorld> World;
    // 潜伏命令持有World强引用期间Actor/组件由世界拥有；Cleanup先撤销能力再拆世界。
    APawn* Pawn = nullptr; AGamePlatformAIController* Controller = nullptr;
    UGamePlatformAbilitySystemComponent* ASC = nullptr;
    FGameplayAbilitySpecHandle SpecHandle;
    FGameplayTagContainer Tags;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIAbilityIntegrationTest, "GamePlatform.AI.AbilityActivation.RealTagChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAIAbilityIntegrationTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FString DefinitionPath, AbilityClassPath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GamePlatformAIAbilityTestDefinition="), DefinitionPath) ||
        !FParse::Value(FCommandLine::Get(), TEXT("GamePlatformAIAbilityTestClass="), AbilityClassPath))
    { AddError(TEXT("需真实开发夹具-GamePlatformAIAbilityTestDefinition=/Game/...Definition 与 -GamePlatformAIAbilityTestClass=/Game/...Ability_C；缺资产不能跳过并报通过。")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FAIAbilityIntegrationCommand(*this, DefinitionPath, AbilityClassPath));
    return true;
}
#endif
