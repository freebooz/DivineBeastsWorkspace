// 开发插件总览启动策略。无持久状态、无后端连接，正式发布构建始终拒绝。
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"

const TCHAR* FDivineBeastsPluginDemoMode::GetHubMapPackage()
{
    return TEXT("/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub");
}

const TCHAR* FDivineBeastsPluginDemoMode::GetCameraReviewMapPackage()
{
    return TEXT("/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview");
}
bool FDivineBeastsPluginDemoMode::IsAllowedArguments(bool bDevelopment, bool bInteractiveClient,
    bool bExplicitOptIn, bool bConflictingDevelopmentMode, const FString& RequestedEntryMap)
{
    // 不采用子串/模糊地图匹配，避免在正式登录地图误开放演示旁路。
    return bDevelopment && bInteractiveClient && bExplicitOptIn
        && !bConflictingDevelopmentMode
        // 允许实际插件总览或独立电影镜头评审地图；拒绝其它正式业务地图。
        && (RequestedEntryMap.Equals(GetHubMapPackage(), ESearchCase::CaseSensitive)
            || RequestedEntryMap.Equals(GetCameraReviewMapPackage(), ESearchCase::CaseSensitive));
}

bool FDivineBeastsPluginDemoMode::IsEnabled()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return false;
#else
    const TCHAR* Args = FCommandLine::Get();
    FString Map;
    const bool bOptIn = FParse::Param(Args, TEXT("DBAPluginDemo"))
        && FParse::Value(Args, TEXT("DBAPluginDemoMap="), Map);
    const bool bConflict = FParse::Param(Args, TEXT("FoundationStandalone"))
        || FParse::Param(Args, TEXT("FoundationOnlineIntegration"));
    return IsAllowedArguments(true,
        !IsRunningDedicatedServer() && !IsRunningCommandlet(),
        bOptIn, bConflict, Map);
#endif
}

bool FDivineBeastsPluginDemoMode::IsValidPluginIdText(const FString& Id)
{
    if (Id.IsEmpty() || Id.Len() >= NAME_SIZE || Id.Equals(TEXT("None"), ESearchCase::IgnoreCase)) return false;
    for (int32 Index = 0; Index < Id.Len(); ++Index)
    {
        const TCHAR C = Id[Index];
        const bool bLetter = (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z'));
        if (!bLetter && !(Index > 0 && ((C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_')))) return false;
    }
    return true;
}
bool FDivineBeastsPluginDemoMode::IsAllowedPluginTestArguments(bool bDevelopment, bool bInteractiveClient,
    bool bExplicitOptIn, bool bConflictingDevelopmentMode, const FString& Map, const FString& Id)
{
    return IsAllowedArguments(bDevelopment, bInteractiveClient, bExplicitOptIn, bConflictingDevelopmentMode, Map)
        && Map.Equals(GetHubMapPackage(), ESearchCase::CaseSensitive) && IsValidPluginIdText(Id);
}
FName FDivineBeastsPluginDemoMode::GetRequestedPluginTest(FString* OutError)
{
    if (OutError) OutError->Reset();
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return NAME_None;
#else
    FString Id, Map;
    const TCHAR* Args = FCommandLine::Get();
    if (!FParse::Value(Args, TEXT("DBAPluginTest="), Id)) return NAME_None;
    const bool bOptIn = FParse::Param(Args, TEXT("DBAPluginDemo")) && FParse::Value(Args, TEXT("DBAPluginDemoMap="), Map);
    const bool bConflict = FParse::Param(Args, TEXT("FoundationStandalone")) || FParse::Param(Args, TEXT("FoundationOnlineIntegration"));
    if (IsAllowedPluginTestArguments(true, !IsRunningDedicatedServer() && !IsRunningCommandlet(), bOptIn, bConflict, Map, Id)) return FName(*Id);
    if (OutError) *OutError = TEXT("DBAPluginTest要求合法PluginId、显式DBAPluginDemo和精确Hub地图资格。");
    return NAME_None;
#endif
}
