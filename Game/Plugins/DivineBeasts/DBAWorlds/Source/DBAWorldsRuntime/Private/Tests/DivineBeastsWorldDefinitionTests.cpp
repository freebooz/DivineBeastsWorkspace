#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/DivineBeastsWorldDefinition.h"
#include "Definitions/GamePlatformExperienceDefinition.h"
#include "Definitions/GamePlatformPawnDefinition.h"

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FGamePlatformId ParseWorldIdentity(const TCHAR* Value)
{
    FGamePlatformId Identity;
    FGamePlatformId::TryParse(Value, Identity);
    return Identity;
}

void ConfigureProjectWorld(
    UDivineBeastsWorldDefinition& Definition,
    const TCHAR* Role,
    const TCHAR* Experience)
{
    Definition.LogicalId = ParseWorldIdentity(TEXT("divinebeasts.world.test@1"));
    Definition.MapIdentity = TSoftObjectPtr<UWorld>(
        FSoftObjectPath(TEXT("/Game/Tests/World.World")));
    Definition.ServerRoleId = Role;
    Definition.DefaultExperienceId = ParseWorldIdentity(Experience);
}
}

// 角色和体验关系来自Shared生成目录；本测试不声称软引用地图资产真实存在。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsWorldRoleExperienceTest,
    "DivineBeasts.Worlds.Definition.RoleExperience",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWorldRoleExperienceTest::RunTest(const FString&)
{
    struct FValidWorldContext
    {
        const TCHAR* Role;
        const TCHAR* Experience;
    };
    const FValidWorldContext ValidContexts[] = {
        {TEXT("GameServer.Role.OpenWorld"), TEXT("experience.openworld.hub@1")},
        // 兼容旧体验身份，但其服务器仍是OpenWorld，不能恢复独立Lobby角色。
        {TEXT("GameServer.Role.OpenWorld"), TEXT("experience.lobby.main@1")},
        {TEXT("GameServer.Role.Village"), TEXT("experience.village.main@1")},
        {TEXT("GameServer.Role.Village"), TEXT("experience.village.tutorial@1")},
        {TEXT("GameServer.Role.Village"), TEXT("experience.village.training@1")},
        {TEXT("GameServer.Role.OpenWorld"), TEXT("experience.openworld.main@1")},
        {TEXT("GameServer.Role.MainArena"), TEXT("experience.mainarena.main@1")},
    };

    for (const FValidWorldContext& Context : ValidContexts)
    {
        TStrongObjectPtr<UDivineBeastsWorldDefinition> Definition(
            NewObject<UDivineBeastsWorldDefinition>());
        ConfigureProjectWorld(*Definition, Context.Role, Context.Experience);
        TestTrue(TEXT("正式角色与体验映射通过"), Definition->ValidateDefinition().IsSuccess());
    }

    TStrongObjectPtr<UDivineBeastsWorldDefinition> Mismatch(
        NewObject<UDivineBeastsWorldDefinition>());
    ConfigureProjectWorld(
        *Mismatch,
        TEXT("GameServer.Role.Village"),
        TEXT("experience.lobby.main@1"));
    TestEqual(
        TEXT("角色体验错配拒绝"),
        Mismatch->ValidateDefinition().Code,
        FName(TEXT("ProjectRoleExperienceMismatch")));

    ConfigureProjectWorld(
        *Mismatch,
        TEXT("GameServer.Role.Lobby"),
        TEXT("experience.lobby.main@1"));
    TestEqual(
        TEXT("旧独立Lobby角色明确拒绝，不能借兼容体验重新启用"),
        Mismatch->ValidateDefinition().Code,
        FName(TEXT("InvalidProjectServerRole")));
    return true;
}

// 竞技模式是可选世界上下文；只有Shared映射的MainArena组合能通过。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsWorldArenaContextTest,
    "DivineBeasts.Worlds.Definition.ArenaContext",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWorldArenaContextTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsWorldDefinition> Definition(
        NewObject<UDivineBeastsWorldDefinition>());
    ConfigureProjectWorld(
        *Definition,
        TEXT("GameServer.Role.MainArena"),
        TEXT("experience.mainarena.main@1"));
    Definition->ArenaModeId = TEXT("Arena.Mode.Team5v5");
    TestTrue(TEXT("正式竞技模式映射通过"), Definition->ValidateDefinition().IsSuccess());

    ConfigureProjectWorld(
        *Definition,
        TEXT("GameServer.Role.OpenWorld"),
        TEXT("experience.openworld.hub@1"));
    TestEqual(
        TEXT("合法OpenWorld大厅也不能携带竞技模式"),
        Definition->ValidateDefinition().Code,
        FName(TEXT("ArenaModeWorldContextMismatch")));

    ConfigureProjectWorld(
        *Definition,
        TEXT("GameServer.Role.MainArena"),
        TEXT("experience.mainarena.main@1"));
    Definition->ArenaModeId = TEXT("Arena.Mode.Unknown");
    TestEqual(
        TEXT("未知竞技模式拒绝"),
        Definition->ValidateDefinition().Code,
        FName(TEXT("InvalidProjectArenaMode")));
    return true;
}

// 黑屏回归：验证实际交付的新手村定义，而不是Transient替身；漏填Purpose会使真实Data租约失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageDeliveredDefinitionTest,
    "DivineBeasts.Worlds.Delivery.VillageGameplayDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageDeliveredDefinitionTest::RunTest(const FString&)
{
    auto* Experience = LoadObject<UGamePlatformExperienceDefinition>(nullptr,
        TEXT("/DBAWorldPack_Village/Definitions/DA_DBA_Experience_Village_Tutorial.DA_DBA_Experience_Village_Tutorial"));
    auto* Pawn = LoadObject<UGamePlatformPawnDefinition>(nullptr,
        TEXT("/DBAWorldPack_Village/Definitions/DA_DBA_Pawn_WorldCharacter.DA_DBA_Pawn_WorldCharacter"));
    if (!TestNotNull(TEXT("真实体验资产已交付"), Experience) || !TestNotNull(TEXT("真实Pawn资产已交付"), Pawn)) return false;
    const auto ExperienceResult = Experience->ValidateDefinition();
    TestTrue(*FString::Printf(TEXT("体验必须通过运行期验证，实际错误=%s"), *ExperienceResult.Code.ToString()), ExperienceResult.IsSuccess());
    TestTrue(TEXT("真实Pawn定义必须通过运行期验证"), Pawn->ValidateDefinition().IsSuccess());
    TestEqual(TEXT("体验指向同一真实Pawn身份"), Experience->DefaultPawnDefinitionId, Pawn->GetPrimaryAssetId());
    return true;
}

#endif
