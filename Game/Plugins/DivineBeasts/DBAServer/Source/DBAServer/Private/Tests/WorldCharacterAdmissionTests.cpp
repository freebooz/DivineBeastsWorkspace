// 项目服务器资料边界回归：响应字符串仅测试夹具，不联网、不生成准入、不作为真实玩家资料。
// 验证选择/拥有/Active/Catalog/唯一性及失败清空输出；生产PlayerId仍必须来自当前真实准入。
#if WITH_DEV_AUTOMATION_TESTS
#include "Characters/DivineBeastsWorldCharacterAdmission.h"
#include "Misc/AutomationTest.h"
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
#endif
