// SSLM项目开发来源观察：只读当前进程及Core模块管理器，不加载/重编译模块，不修改业务或资源。
// 所有文件观察是端点事实，不是持续内存证明；来源缺失时维持unverified，机器资格由独立工具判定。
#include "Evidence/DivineBeastsDemoRuntimeOrigin.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Hash/GamePlatformSHA256.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

namespace DivineBeasts::Demo
{
FString HashRuntimeOriginFile(const FString& Path)
{
    auto& Files = FPlatformFileManager::Get().GetPlatformFile();
    bool bPlain = true;
    for (FString Cursor = Path; !Cursor.IsEmpty();)
    {
        if ((Files.FileExists(*Cursor) || Files.DirectoryExists(*Cursor)) && Files.IsSymlink(*Cursor) != ESymlinkResult::NonSymlink) { bPlain = false; break; }
        const FString Parent = FPaths::GetPath(Cursor); if (Parent == Cursor) break; Cursor = Parent;
    }
    // 64MiB是单次诊断缓冲保护，不是项目DLL/性能预算；超限拒绝补证，不截断后散列。
    const int64 Size = Files.FileSize(*Path); const auto Timestamp = Files.GetTimeStamp(*Path); TArray<uint8> Bytes;
    if (!bPlain || Size <= 0 || Size > 64 * 1024 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *Path) ||
        Bytes.Num() != Size || Files.FileSize(*Path) != Size || Files.GetTimeStamp(*Path) != Timestamp) return FString();
    FString Hash;
    return GamePlatform::Hash::TrySha256Hex(Bytes.GetData(), Bytes.Num(), Hash) ? Hash : FString();
}
FGamePlatformResult ParseRuntimeOriginArguments(const TCHAR* Arguments, FGuid& OutLaunchId, FString& OutContentHash)
{
    OutLaunchId.Invalidate(); OutContentHash.Reset(); FString Launch, Hash; bool bLiveCodingEnabled = true;
    if (!Arguments || !FParse::Value(Arguments, TEXT("DBAPluginDemoLaunchId="), Launch) ||
        !FGuid::ParseExact(Launch, EGuidFormats::DigitsWithHyphens, OutLaunchId) || !OutLaunchId.IsValid() ||
        !FParse::Value(Arguments, TEXT("DBAPluginDemoContentFingerprint="), Hash) || Hash.Len() != 64 ||
        !FParse::Bool(Arguments, TEXT("-LiveCoding="), bLiveCodingEnabled) || bLiveCodingEnabled || !FParse::Param(Arguments, TEXT("NoHotReload")))
    {
        OutLaunchId.Invalidate(); return FGamePlatformResult::Failure(TEXT("Demo.UnverifiedLaunch"), TEXT("缺受控启动身份、内容端点或禁热重载参数。"));
    }
    for (TCHAR C : Hash) if (!(C >= TEXT('0') && C <= TEXT('9')) && !(C >= TEXT('a') && C <= TEXT('f')))
    { OutLaunchId.Invalidate(); return FGamePlatformResult::Failure(TEXT("Demo.InvalidOriginHash"), TEXT("启动内容摘要不是规范SHA256。")); }
    OutContentHash = Hash; return FGamePlatformResult::Success();
}
TSharedRef<FJsonObject> CaptureRuntimeOrigin()
{
    check(IsInGameThread()); auto Origin = MakeShared<FJsonObject>(); FGuid Launch; FString Content;
    const auto Parsed = ParseRuntimeOriginArguments(FCommandLine::Get(), Launch, Content);
    Origin->SetStringField(TEXT("launch_id"), Launch.IsValid() ? Launch.ToString(EGuidFormats::DigitsWithHyphens) : FString());
    Origin->SetStringField(TEXT("content_fingerprint"), Content);
    // UE5.8运行时消费-LiveCoding=false，而-NoLiveCoding是构建侧参数，不能由后者推断会话禁用。
    // 只观察已存在模块，不强载/启停会话。已启动后再隐藏/停用仍HasStarted，不能给旧补丁补资格。
    bool bLiveCodingStateVerified = true;
#if WITH_LIVE_CODING
    const auto* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
    bLiveCodingStateVerified = LiveCoding && !LiveCoding->HasStarted() && !LiveCoding->IsEnabledForSession() && !LiveCoding->IsCompiling();
#endif
    Origin->SetBoolField(TEXT("no_live_coding"), Parsed.IsSuccess() && bLiveCodingStateVerified);
    Origin->SetBoolField(TEXT("live_coding_state_verified"), bLiveCodingStateVerified);
    // 不把构建侧NoHotReload标志当引擎阻断事实；检查当前模块是否出现过文件名替换，单体不补资格。
    bool bHotReloadStateVerified = false;
#if !IS_MONOLITHIC
    bHotReloadStateVerified = !FModuleManager::Get().HasAnyOverridenModuleFilename();
#endif
    Origin->SetBoolField(TEXT("no_hot_reload"), FParse::Param(FCommandLine::Get(), TEXT("NoHotReload")) && bHotReloadStateVerified);
    Origin->SetBoolField(TEXT("hot_reload_state_verified"), bHotReloadStateVerified);
    Origin->SetStringField(TEXT("diagnostic_code"), Parsed.Code.ToString());
    if (Parsed.IsSuccess() && !bLiveCodingStateVerified) Origin->SetStringField(TEXT("diagnostic_code"), TEXT("Demo.LiveCodingOriginUnverified"));
    TArray<TSharedPtr<FJsonValue>> Observations;
    if (Parsed.IsSuccess())
    {
#if !IS_MONOLITHIC
        // 仅观察来源证明需要的机制模块；不因同名模块存在就推断所有源码或DLL已更新。
        FString Workspace = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("..")));
        FPaths::NormalizeDirectoryName(Workspace); Workspace += TEXT("/");
        auto& Modules = FModuleManager::Get();
        // -game不会加载Type=Editor审核模块；它由独立Editor Review组证明，不能反向强载到游戏运行。
        for (const TCHAR* Name : {TEXT("DivineBeastsDemoClient"), TEXT("DivineBeastsApplicationFlowClient"), TEXT("GamePlatformData"), TEXT("GamePlatformUIClient")})
        {
            if (!Modules.IsModuleLoaded(FName(Name))) continue;
            FString Path = FPaths::ConvertRelativePathToFull(Modules.GetModuleFilename(FName(Name))); FPaths::MakeStandardFilename(Path);
            if (!Path.StartsWith(Workspace, ESearchCase::IgnoreCase)) continue;
            const FString Hash = HashRuntimeOriginFile(Path); if (Hash.IsEmpty()) continue;
            auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("name"), Name);
            Entry->SetStringField(TEXT("path"), Path.RightChop(Workspace.Len())); Entry->SetStringField(TEXT("sha256"), Hash);
            Observations.Add(MakeShared<FJsonValueObject>(Entry));
        }
#else
        // UE5.8单体目标没有动态模块文件名接口；不能拿Editor DLL或源代码指纹冒充Client实际产物。
        // 此路径仍可保留原始运行事实，但独立Client可执行文件/Stage证明实现前拒绝提升机器资格。
        Origin->SetStringField(TEXT("diagnostic_code"), TEXT("Demo.UnsupportedMonolithicOrigin"));
#endif
    }
    Origin->SetArrayField(TEXT("loaded_modules"), Observations); return Origin;
}
}
