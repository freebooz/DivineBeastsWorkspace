// SSLM登录开发入口桥。只启动工作空间固定脚本，完整构建门禁仍由既有脚本负责。
// 不链接DivineBeastsDemoClient，避免Demo→UI→Demo环；不制造认证成功或进程内模式切换。
#include "Development/DivineBeastsPluginDemoLauncher.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

bool UDivineBeastsPluginDemoLauncher::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer() && !IsRunningCommandlet();
#else
    return false;
#endif
}

FString UDivineBeastsPluginDemoLauncher::GetScriptPath() const
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Build/Game/LaunchPluginDemoFromLogin.ps1"));
}

bool UDivineBeastsPluginDemoLauncher::IsEntryAvailable() const
{
#if WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && !World->bIsTearingDown && World->GetNetMode() == NM_Standalone &&
        !IsRunningDedicatedServer() && !IsRunningCommandlet() && !FDivineBeastsPluginDemoMode::IsEnabled() &&
        IFileManager::Get().FileExists(*GetScriptPath());
#else
    return false;
#endif
}

void UDivineBeastsPluginDemoLauncher::SetStatus(const FText& Text)
{
    StatusText = Text;
    OnStatusChanged.Broadcast();
}

void UDivineBeastsPluginDemoLauncher::RequestLaunch()
{
    check(IsInGameThread());
    if (!IsEntryAvailable() || IsBusy()) { return; }
    FString PowerShell;
    // 开发启动器传递其真实PS7宿主位置；不使用PATH搜寻任意同名命令，也不执行拼接Shell。
    FParse::Value(FCommandLine::Get(), TEXT("DBADevelopmentPowerShell="), PowerShell);
    if (PowerShell.IsEmpty())
    {
        PowerShell = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles")) / TEXT("PowerShell/7/pwsh.exe");
    }
    const FString EngineRoot = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT(".."));
    const FString Script = GetScriptPath();
    const auto IsSafeArgument = [](const FString& Value)
    { return !Value.Contains(TEXT("\"")) && !Value.Contains(TEXT("\r")) && !Value.Contains(TEXT("\n")); };
    if (!FPaths::GetCleanFilename(PowerShell).Equals(TEXT("pwsh.exe"), ESearchCase::IgnoreCase) ||
        !IFileManager::Get().FileExists(*PowerShell) || !IsSafeArgument(PowerShell) || !IsSafeArgument(Script) || !IsSafeArgument(EngineRoot))
    {
        SetStatus(FText::FromString(TEXT("需要PowerShell 7。请使用LaunchDBADevelopment.ps1启动调试登录，再点击此入口。")));
        return;
    }
    LaunchId = FGuid::NewGuid(); LastStatus.Reset();
    StatusFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Saved/Validation/PluginDemos/LoginLaunch") /
        LaunchId.ToString(EGuidFormats::DigitsWithHyphens) / TEXT("status.txt"));
    const FString Arguments = FString::Printf(TEXT("-NoLogo -NoProfile -NonInteractive -File \"%s\" -EngineRoot \"%s\" -RunId %s"),
        *Script, *EngineRoot, *LaunchId.ToString(EGuidFormats::DigitsWithHyphens));
    // 隐藏工具窗口；实际演示由受控脚本打开可见游戏窗口，不传递父进程参数或登录凭据。
    Process = FPlatformProcess::CreateProc(*PowerShell, *Arguments, true, true, true, nullptr, 0, *FPaths::ProjectDir(), nullptr);
    if (!Process.IsValid())
    {
        SetStatus(FText::FromString(TEXT("未能启动插件开发检查；登录窗口保持不变。")));
        return;
    }
    PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::PollProcess), 0.5f);
    SetStatus(FText::FromString(TEXT("正在检查完整构建证据和演示资源，请稍候；不会跳过认证或自动构建。")));
}

bool UDivineBeastsPluginDemoLauncher::PollProcess(float)
{
    check(IsInGameThread());
    const bool bRunning = Process.IsValid() && FPlatformProcess::IsProcRunning(Process);
    FString Current;
    if (IFileManager::Get().FileSize(*StatusFile) >= 0 && IFileManager::Get().FileSize(*StatusFile) <= 64)
    { FFileHelper::LoadFileToString(Current, *StatusFile); Current.TrimStartAndEndInline(); }
    if (Current != LastStatus)
    {
        LastStatus = Current;
        if (Current == TEXT("Started"))
        { SetStatus(FText::FromString(TEXT("插件演示进程已启动，请切换到新窗口。门户就绪及验证结果以演示窗口为准。"))); }
    }
    if (bRunning) { return true; }
    int32 ExitCode = -1; FPlatformProcess::GetProcReturnCode(Process, &ExitCode);
    FPlatformProcess::CloseProc(Process); Process.Reset(); PollHandle.Reset();
    if (ExitCode == 0 && Current == TEXT("Exited"))
    { SetStatus(FText::FromString(TEXT("插件演示进程已退出，可以重新启动；原登录流程未改变。"))); }
    else
    {
        SetStatus(FText::FromString(FString::Printf(TEXT("启动检查或演示进程失败，未切换登录模式。查看Saved/Validation/PluginDemos/LoginLaunch/%s；先核对完整Editor构建证据与资源。"),
            *LaunchId.ToString(EGuidFormats::DigitsWithHyphens))));
    }
    return false;
}

void UDivineBeastsPluginDemoLauncher::Deinitialize()
{
    if (PollHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(PollHandle); PollHandle.Reset(); }
    OnStatusChanged.Clear();
    if (Process.IsValid()) { FPlatformProcess::CloseProc(Process); Process.Reset(); }
    Super::Deinitialize();
}
