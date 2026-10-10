#if WITH_DEV_AUTOMATION_TESTS
// 项目组合根在ASC已绑定/随后换Avatar/结束时真实接入和撤销；无就绪和Active事实不能被假成功放行。
#include "Misc/AutomationTest.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterActivationLifecycleTest,
    "DivineBeasts.Characters.ActivationGate.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCharacterActivationLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Pawn = World->SpawnActor<ACharacter>(); auto* OtherPawn = World->SpawnActor<ACharacter>();
    auto* ASC = NewObject<UGamePlatformAbilitySystemComponent>(Pawn); Pawn->AddInstanceComponent(ASC); ASC->RegisterComponent();
    ASC->BindAbilityActorInfo(Pawn, Pawn);
    auto* Character = NewObject<UDivineBeastsCharacterComponent>(Pawn); Pawn->AddInstanceComponent(Character);
    Character->RegisterComponent(); Character->BeginPlay();
    TestEqual(TEXT("真实项目Gate已注入但角色未Ready必须拒绝"), ASC->EvaluateActivationEligibility().Code, FName(TEXT("CharacterNotReady")));
    ASC->BindAbilityActorInfo(Pawn, OtherPawn);
    TestFalse(TEXT("旧角色组件不能授权另一个Avatar"), ASC->EvaluateActivationEligibility().IsSuccess());
    ASC->BindAbilityActorInfo(Pawn, Pawn);
    TestEqual(TEXT("重新绑定事件重新注入只读Gate"), ASC->EvaluateActivationEligibility().Code, FName(TEXT("CharacterNotReady")));
    Character->EndPlay(EEndPlayReason::Destroyed);
    TestEqual(TEXT("组件结束撤销注入"), ASC->EvaluateActivationEligibility().Code, FName(TEXT("ActivationGateUnavailable")));
    World->DestroyWorld(false); return true;
}

namespace
{
// 只验证真实身份绑定通知的运行所有权；没有GameInstance/Data/英雄资产，接纳身份不代表Definition Ready。
struct FTrustedContextLifecycleFixture
{
    UWorld* World = nullptr;
    ADivineBeastsCharacter* Pawn = nullptr;
    ~FTrustedContextLifecycleFixture()
    { if (World) { if (IsValid(Pawn)) { Pawn->Destroy(); } World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); } }
    bool Initialize(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        if (!Test.TestNotNull(TEXT("可信绑定测试世界"), World)) { return false; }
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters Parameters; Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<ADivineBeastsCharacter>(Parameters);
        if (!Test.TestNotNull(TEXT("可信绑定真实Pawn"), Pawn)) { return false; }
        Pawn->DispatchBeginPlay(); return true;
    }
};
}

// 高代次后继在false通知内同步接管，原栈必须返回取消且不能写回旧身份/代次。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsTrustedContextSuccessorTest,
    "DivineBeasts.Characters.TrustedContext.ReentrantSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsTrustedContextSuccessorTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FTrustedContextLifecycleFixture Fixture; if (!Fixture.Initialize(*this)) { return false; }
    auto* Identity = Fixture.Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    const auto& Heroes = FDivineBeastsHeroCatalog::GetCoreHeroIds();
    if (!TestNotNull(TEXT("真实身份组件"), Identity) || !TestTrue(TEXT("真实核心目录存在"), !Heroes.IsEmpty())) { return false; }
    FGamePlatformCharacterInitializationContext First; First.CharacterId = TEXT("Test.First.Character");
    First.HeroDefinitionId = Heroes[0]; First.SpawnGeneration = 1; First.AvatarGeneration = 1;
    auto Successor = First; Successor.CharacterId = TEXT("Test.Successor.Character"); Successor.SpawnGeneration = 2; Successor.AvatarGeneration = 2;
    bool bReentered = false; bool bSuccessorAccepted = false; int32 Revocations = 0;
    const auto Handle = Identity->OnReadinessChanged().AddLambda([&](bool bReady)
    {
        if (bReady) { return; } ++Revocations;
        if (!bReentered) { bReentered = true; FString Error; bSuccessorAccepted = Identity->AuthorityBindTrustedContext(Successor, Error); }
    });
    FString Error; TestFalse(TEXT("原绑定被真实后继撤销"), Identity->AuthorityBindTrustedContext(First, Error));
    Identity->OnReadinessChanged().Remove(Handle);
    TestTrue(TEXT("后继可信身份绑定已受理"), bSuccessorAccepted);
    TestEqual(TEXT("后继持久身份未被旧栈覆盖"), Identity->GetCharacterId(), Successor.CharacterId);
    TestEqual(TEXT("后继出生代次保持2"), Identity->GetSpawnGeneration(), 2);
    TestEqual(TEXT("后继Avatar代次保持2"), Identity->GetAvatarGeneration(), 2);
    TestEqual(TEXT("每个接纳绑定只有一次撤销通知"), Revocations, 2);
    TestFalse(TEXT("缺真实Data/Definition不得伪称Ready"), Identity->IsCharacterReady()); return true;
}

// 结束可发生在已发布未Ready身份的同步撤销通知中，原栈不得继续初始化；关闭后的入口不能推进身份。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsTrustedContextCloseTest,
    "DivineBeasts.Characters.TrustedContext.CloseDuringNotification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsTrustedContextCloseTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FTrustedContextLifecycleFixture Fixture; if (!Fixture.Initialize(*this)) { return false; }
    auto* Identity = Fixture.Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    const auto& Heroes = FDivineBeastsHeroCatalog::GetCoreHeroIds();
    if (!TestNotNull(TEXT("真实身份组件"), Identity) || !TestTrue(TEXT("真实核心目录存在"), !Heroes.IsEmpty())) { return false; }
    FGamePlatformCharacterInitializationContext Context; Context.CharacterId = TEXT("Test.Close.Character");
    Context.HeroDefinitionId = Heroes[0]; Context.SpawnGeneration = 1; Context.AvatarGeneration = 1;
    int32 Revocations = 0;
    const auto Handle = Identity->OnReadinessChanged().AddLambda([&](bool bReady)
    { if (!bReady) { ++Revocations; Identity->EndPlay(EEndPlayReason::Destroyed); } });
    FString Error; TestFalse(TEXT("通知关闭撤销原绑定"), Identity->AuthorityBindTrustedContext(Context, Error));
    Identity->OnReadinessChanged().Remove(Handle);
    ++Context.SpawnGeneration; ++Context.AvatarGeneration;
    TestFalse(TEXT("已结束组件拒绝后续可信绑定"), Identity->AuthorityBindTrustedContext(Context, Error));
    TestEqual(TEXT("关闭仅发一次本次撤销通知"), Revocations, 1);
    TestEqual(TEXT("关闭之后没有推进Avatar身份"), Identity->GetAvatarGeneration(), 1);
    TestFalse(TEXT("关闭之后不再进行无GI的定义初始化"), Identity->GetLastDefinitionLoadResult().Code == FName(TEXT("DataGameInstanceUnavailable")));
    TestFalse(TEXT("关闭后Ready失败关闭"), Identity->IsCharacterReady()); return true;
}
#endif
