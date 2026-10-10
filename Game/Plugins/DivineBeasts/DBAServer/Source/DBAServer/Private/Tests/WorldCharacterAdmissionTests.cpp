// 项目服务器资料边界回归：响应字符串仅测试夹具，不联网、不生成准入、不作为真实玩家资料。
// 验证选择/拥有/Active/Catalog/唯一性及失败清空输出；生产PlayerId仍必须来自当前真实准入。
#if WITH_DEV_AUTOMATION_TESTS
#include "Characters/DivineBeastsWorldCharacterAdmission.h"
#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCharacterAdmissionRosterTest, "DivineBeasts.Server.WorldCharacterAdmission.Roster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCharacterAdmissionRosterTest::RunTest(const FString&)
{
    using namespace DivineBeasts::WorldCharacterAdmission;
    FString Selected = TEXT("stale"); FName Hero(TEXT("stale"));
    const FString Profile = TEXT(R"({"found":true,"playerId":"fixture-player","selectedCharacterId":"fixture-b","ownedCharacterIds":["fixture-a","fixture-b"]})");
    TestTrue(TEXT("可信玩家选择必须属于拥有列表"), ParseSelectedCharacter(Profile, TEXT("fixture-player"), Selected));
    TestEqual(TEXT("使用选择而非首角色"), Selected, FString(TEXT("fixture-b")));
    TestFalse(TEXT("拒绝错玩家响应"), ParseSelectedCharacter(Profile, TEXT("other"), Selected));
    TestTrue(TEXT("失败清空旧CharacterID"), Selected.IsEmpty());
    TestFalse(TEXT("缺少资料拒绝"), ParseSelectedCharacter(TEXT("{}"), TEXT("fixture-player"), Selected));
    TestFalse(TEXT("未拥有选中角色拒绝"), ParseSelectedCharacter(TEXT(R"({"found":true,"playerId":"fixture-player","selectedCharacterId":"foreign","ownedCharacterIds":["fixture-a"]})"), TEXT("fixture-player"), Selected));
    const FString Roster = TEXT(R"({"characters":[{"characterId":"fixture-a","status":"Active","heroDefinitionId":"Hero.Zodiac.Rat"},{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Zodiac.Tiger"}]})");
    TestTrue(TEXT("选择角色映射到真实核心Hero"), ParseSelectedHero(Roster, TEXT("fixture-b"), Hero));
    TestEqual(TEXT("不是首英雄固定回退"), Hero, FName(TEXT("Hero.Zodiac.Tiger")));
    TestFalse(TEXT("角色不在该玩家Roster拒绝"), ParseSelectedHero(Roster, TEXT("foreign"), Hero));
    TestTrue(TEXT("失败清空旧Hero"), Hero.IsNone());
    TestFalse(TEXT("重复角色身份不能任意选一个"), ParseSelectedHero(TEXT(R"({"characters":[{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Zodiac.Tiger"},{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Zodiac.Rat"}]})"), TEXT("fixture-b"), Hero));
    TestFalse(TEXT("Disabled拒绝"), ParseSelectedHero(TEXT(R"({"characters":[{"characterId":"fixture-b","status":"Disabled","heroDefinitionId":"Hero.Zodiac.Tiger"}]})"), TEXT("fixture-b"), Hero));
    TestFalse(TEXT("未知Hero拒绝，不推算资产路径"), ParseSelectedHero(TEXT(R"({"characters":[{"characterId":"fixture-b","status":"Active","heroDefinitionId":"Hero.Unknown"}]})"), TEXT("fixture-b"), Hero));
    TestFalse(TEXT("损坏JSON拒绝"), ParseSelectedHero(TEXT("invalid-json"), TEXT("fixture-b"), Hero));
    return true;
}
// 使用真实Controller/Pawn/身份组件，无GameInstance数据服务是明确失败夹具；不注册或签发生产服务器准入。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCharacterAdmissionRequiredDefinitionTest, "DivineBeasts.Server.WorldCharacterAdmission.RequiredDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldCharacterAdmissionRequiredDefinitionTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("真实瞬态世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* Character = World->SpawnActor<ADivineBeastsCharacter>();
    if (!TestNotNull(TEXT("真实Controller"), Controller) || !TestNotNull(TEXT("真实项目角色"), Character)) return false;
    TestFalse(TEXT("未绑定Pawn不能激活"), DivineBeasts::WorldCharacterAdmission::IsPawnReadyForActivation(*Controller));
    Controller->Possess(Character);
    auto* Identity = Character->FindComponentByClass<UDivineBeastsCharacterComponent>();
    FGamePlatformCharacterInitializationContext Context;
    Context.CharacterId = TEXT("required-definition-fixture"); Context.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Context.SpawnGeneration = 1; Context.AvatarGeneration = 1; FString Error;
    TestTrue(TEXT("身份受理仍可能异步初始化失败"), Identity->AuthorityBindTrustedContext(Context, Error));
    TestFalse(TEXT("缺真实Data服务不会伪装为Ready"), Identity->IsCharacterReady());
    TestFalse(TEXT("身份受理不等于允许Active"), DivineBeasts::WorldCharacterAdmission::IsPawnReadyForActivation(*Controller));
    TestEqual(TEXT("缺服务保留真实失败诊断"), Identity->GetLastDefinitionLoadResult().Code, FName(TEXT("DataGameInstanceUnavailable")));
    Controller->UnPossess();
    TestFalse(TEXT("撤销Pawn后不能重新激活"), DivineBeasts::WorldCharacterAdmission::IsPawnReadyForActivation(*Controller));
    return true;
}
#endif
