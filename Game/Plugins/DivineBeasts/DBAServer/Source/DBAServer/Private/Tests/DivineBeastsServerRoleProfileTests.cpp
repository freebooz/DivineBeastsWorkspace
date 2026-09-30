#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Server/DivineBeastsServerRoleProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsServerRoleProfileLoadTest,
    "DivineBeasts.Server.Profile.LoadAndValidateRoles",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

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
        if (RoleName == TEXT("Village"))
        {
            TestEqual(
                TEXT("Village默认进入新手教学体验"),
                Profile.DefaultExperienceId,
                FName(TEXT("Experience.Village.Tutorial")));
            TestEqual(
                TEXT("Village使用第三层正式世界内容包地图"),
                Profile.WorldPackage,
                FString(TEXT("/DBAWorldPack_Village/Maps/L_Village_Start")));
            TestEqual(
                TEXT("Village至少声明一个Ready必需资源"),
                Profile.RequiredAssets.Num(),
                1);
            if (Profile.RequiredAssets.Num() == 1)
            {
                TestEqual(
                    TEXT("Village Ready必需资源必须是L_Village_Start"),
                    Profile.RequiredAssets[0].ToString(),
                    FString(TEXT("/DBAWorldPack_Village/Maps/L_Village_Start.L_Village_Start")));
            }
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
