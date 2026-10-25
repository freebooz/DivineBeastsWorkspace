// SSLM人型怪物移动配置回归：仅人型吸血鬼追平正常玩家基础速度，不改变狼和友好NPC的默认速度。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsVillageVampire.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Characters/DivineBeastsVillageNPC.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageHumanoidSpeedTest,
    "DivineBeasts.Village.Monsters.HumanoidBaseSpeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsVillageHumanoidSpeedTest::RunTest(const FString&)
{
    const auto* Humanoid = GetDefault<ADivineBeastsVillageVampire>();
    const auto* Wolf = GetDefault<ADivineBeastsVillageWolf>();
    const auto* FriendlyNPC = GetDefault<ADivineBeastsVillageNPC>();
    if (!TestNotNull(TEXT("人型怪物默认类"), Humanoid) ||
        !TestNotNull(TEXT("狼怪物默认类"), Wolf) ||
        !TestNotNull(TEXT("友好村民默认类"), FriendlyNPC))
        return false;
    TestEqual(TEXT("人型怪物构造基线速度为450厘米每秒"),
        Humanoid->GetCharacterMovement()->MaxWalkSpeed, 450.0f);
    TestEqual(TEXT("普通狼原有移动速度保持375厘米每秒"),
        Wolf->GetCharacterMovement()->MaxWalkSpeed, 375.0f);
    TestEqual(TEXT("友好村民原有移动速度保持180厘米每秒"),
        FriendlyNPC->GetCharacterMovement()->MaxWalkSpeed, 180.0f);
    // 追击期速度采用玩家实际HeroDefinition的验证仍需要专服与玩家共同就绪的运行测试。
    return true;
}
#endif
