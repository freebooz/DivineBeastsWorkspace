#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Server/DivineBeastsServerRoleProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsServerRoleProfileLoadTest,
    "DivineBeasts.Server.Profile.LoadAndValidateRoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsServerRoleProfileLoadTest::RunTest(const FString&)
{
    const TArray<FString> RoleNames = {
        TEXT("OpenWorld"), TEXT("Village"), TEXT("MainArena")};
    for (const FString& RoleName : RoleNames)
    {
        FDivineBeastsServerRoleProfile Profile;
        FString Error;
        TestTrue(
            *FString::Printf(TEXT("%s Profile应通过身份与体验映射检查"), *RoleName),
            FDivineBeastsServerRoleProfile::TryLoadForRoleName(RoleName, Profile, Error));
        TestEqual(
            *FString::Printf(TEXT("%s角色身份"), *RoleName),
            Profile.ServerRoleId,
            FName(*(TEXT("GameServer.Role.") + RoleName)));
        if (RoleName == TEXT("MainArena"))
        {
            TestEqual(TEXT("MainArena声明五种竞技模式"), Profile.ArenaModeIds.Num(), 5);
        }
        else
        {
            TestEqual(*FString::Printf(TEXT("%s不声明竞技模式"), *RoleName), Profile.ArenaModeIds.Num(), 0);
        }
        if (RoleName == TEXT("OpenWorld"))
        {
            TestEqual(TEXT("OpenWorld默认进入大厅体验"), Profile.DefaultExperienceId, FName(TEXT("Experience.OpenWorld.Hub")));
        }
    }

    FDivineBeastsServerRoleProfile InvalidProfile;
    FString InvalidRoleError;
    TestFalse(
        TEXT("未知服务器角色不得解析为任何Profile"),
        FDivineBeastsServerRoleProfile::TryLoadForRoleName(
            TEXT("Unknown"), InvalidProfile, InvalidRoleError));
    return true;
}

#endif
