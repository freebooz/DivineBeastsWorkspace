// SSLM登录开发入口桥。只启动工作空间固定脚本，完整构建门禁仍由既有脚本负责。
// 不链接DivineBeastsDemoClient，避免Demo→UI→Demo环；不制造认证成功或进程内模式切换。
#include "Development/DivineBeastsPluginDemoLauncher.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

bool UDivineBeastsPluginDemoLauncher::ShouldCreateSubsystem(UObject* Outer) const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // 编辑器保留既有人工入口；Cook客户端必须由本地开发启动器明确选择，不能随正式启动开放。
    const bool bHostOptIn = WITH_EDITOR || FParse::Param(FCommandLine::Get(), TEXT("DBADevelopmentTools"));
    return Super::ShouldCreateSubsystem(Outer) && bHostOptIn && !IsRunningDedicatedServer() && !IsRunningCommandlet();
#else
    return false;
#endif
}

bool UDivineBeastsPluginDemoLauncher::TryGetWorkspace(FString& OutWorkspace) const
{
    OutWorkspace.Reset();
    FParse::Value(FCommandLine::Get(), TEXT("DBADevelopmentWorkspace="), OutWorkspace);
#if WITH_EDITOR
    if (OutWorkspace.IsEmpty()) { OutWorkspace = FPaths::ProjectDir() / TEXT(".."); }
#endif
    if (OutWorkspace.IsEmpty() || OutWorkspace.Contains(TEXT("\"")) ||
        OutWorkspace.Contains(TEXT("\r")) || OutWorkspace.Contains(TEXT("\n"))) { return false; }
    OutWorkspace = FPaths::ConvertRelativePathToFull(OutWorkspace);
    FPaths::NormalizeDirectoryName(OutWorkspace);
    FPaths::CollapseRelativeDirectories(OutWorkspace);
    const FString ProjectFile = OutWorkspace / TEXT("Game/DivineBeastsArena.uproject");
    if (FPaths::GetCleanFilename(OutWorkspace) != TEXT("DivineBeastsWorkspace") ||
        FString(FApp::GetProjectName()) != TEXT("DivineBeastsArena") ||
        !IFileManager::Get().FileExists(*ProjectFile) ||
        !IFileManager::Get().FileExists(*(OutWorkspace / TEXT("Build/Game/LaunchPluginDemoFromLogin.ps1")))) { return false; }
#if WITH_EDITOR
    // Editor入口不能借显式参数指向另一份同名工程；打包Client由显式源码根交给统一门禁核验。
    if (!FPaths::IsSamePath(ProjectFile, FPaths::ProjectDir() / TEXT("DivineBeastsArena.uproject"))) { return false; }
#endif
    return true;
}

bool UDivineBeastsPluginDemoLauncher::IsEntryAvailable() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const bool bHostOptIn = WITH_EDITOR || FParse::Param(FCommandLine::Get(), TEXT("DBADevelopmentTools"));
    const UWorld* World = GetWorld();
    FString Workspace;
    return bHostOptIn && World && World->IsGameWorld() && !World->bIsTearingDown && World->GetNetMode() == NM_Standalone &&
        !IsRunningDedicatedServer() && !IsRunningCommandlet() && !FDivineBeastsPluginDemoMode::IsEnabled() && TryGetWorkspace(Workspace);
#else
    return false;
#endif
}

void UDivineBeastsPluginDemoLauncher::SetStatus(const FText& Text)
{
    if (StatusText.EqualTo(Text)) { return; }
    StatusText = Text;
    OnStatusChanged.Broadcast();
}

void UDivineBeastsPluginDemoLauncher::RequestLaunch()
{
    check(IsInGameThread());
    if (!IsEntryAvailable() || IsBusy()) { return; }
    FString Workspace;
    if (!TryGetWorkspace(Workspace)) { return; }
    FString PowerShell;
    // 开发启动器传递其真实PS7宿主位置；不使用PATH搜寻任意同名命令，也不执行拼接Shell。
    FParse::Value(FCommandLine::Get(), TEXT("DBADevelopmentPowerShell="), PowerShell);
    if (PowerShell.IsEmpty())
    {
        PowerShell = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles")) / TEXT("PowerShell/7/pwsh.exe");
    }
    FString EngineRoot;
    FParse::Value(FCommandLine::Get(), TEXT("DBADevelopmentEngineRoot="), EngineRoot);
#if WITH_EDITOR
    if (EngineRoot.IsEmpty()) { EngineRoot = FPaths::EngineDir() / TEXT(".."); }
#endif
    EngineRoot = EngineRoot.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(EngineRoot);
    const FString Script = Workspace / TEXT("Build/Game/LaunchPluginDemoFromLogin.ps1");
    const auto IsSafeArgument = [](const FString& Value)
    { return !Value.Contains(TEXT("\"")) && !Value.Contains(TEXT("\r")) && !Value.Contains(TEXT("\n")); };
    if (!FPaths::GetCleanFilename(PowerShell).Equals(TEXT("pwsh.exe"), ESearchCase::IgnoreCase) ||
        !IFileManager::Get().FileExists(*PowerShell) || !IsSafeArgument(PowerShell) || !IsSafeArgument(Script) || !IsSafeArgument(EngineRoot))
    {
        SetStatus(FText::FromString(TEXT("需要PowerShell 7。请使用LaunchDBADevelopment.ps1启动调试登录，再点击此入口。")));
        return;
    }
    if (EngineRoot.IsEmpty() || !IFileManager::Get().FileExists(*(EngineRoot / TEXT("Engine/Binaries/Win64/UnrealEditor.exe"))))
    {
        SetStatus(FText::FromString(TEXT("缺少真实UE编辑器路径。请使用RunFrontEndClients.ps1的DevelopmentTools、EngineRoot与DevelopmentBuildResult参数启动。")));
        return;
    }
    FString BuildResultPath;
    FParse::Value(FCommandLine::Get(), TEXT("DBADevelopmentBuildResult="), BuildResultPath);
    // 显式指定证据，不猜测旧记录；实际输入和产物一致性仍由唯一启动门禁检查。
    if (BuildResultPath.IsEmpty() || !IsSafeArgument(BuildResultPath) || !IFileManager::Get().FileExists(*BuildResultPath))
    {
        SetStatus(FText::FromString(TEXT("缺少完整Editor构建证据。请使用开发启动器明确传入当前Editor构建结果。")));
        return;
    }
    if (!LaunchState.TryBegin()) { return; }
    const FGuid LaunchId = LaunchState.GetLaunchId();
    const uint64 LaunchGeneration = LaunchState.GetGeneration();
    const FString StatusFile = Workspace / TEXT("Saved/Validation/PluginDemos/LoginLaunch") /
        LaunchId.ToString(EGuidFormats::DigitsWithHyphens) / TEXT("status.txt");
    const FString Arguments = FString::Printf(TEXT("-NoLogo -NoProfile -NonInteractive -File \"%s\" -EngineRoot \"%s\" -RunId %s -BuildResultPath \"%s\""),
        *Script, *EngineRoot, *LaunchId.ToString(EGuidFormats::DigitsWithHyphens), *BuildResultPath);
    // 隐藏工具窗口；实际演示由受控脚本打开可见游戏窗口，不传递父进程参数或登录凭据。
    FProcHandle Process = FPlatformProcess::CreateProc(*PowerShell, *Arguments, true, true, true, nullptr, 0, *(Workspace / TEXT("Game")), nullptr);
    if (!Process.IsValid())
    {
        LaunchState.TryComplete(LaunchId, LaunchGeneration);
        SetStatus(FText::FromString(TEXT("未能启动插件开发检查；登录窗口保持不变。")));
        return;
    }
    SetStatus(FText::FromString(TEXT("正在检查完整Editor构建证据并启动插件门户；门户将在独立窗口打开，关闭后可再次进入。")));
    const TWeakObjectPtr<UDivineBeastsPluginDemoLauncher> WeakThis(this);
    // 一次后台等待由OS进程终止事件唤醒；不使用Tick/Ticker/定时器扫描UI、文件或业务状态。
    // 工作线程独占进程句柄，不持有UObject；GameInstance退出也不终止用户的演示窗口。
    Async(EAsyncExecution::Thread, [WeakThis, Process, LaunchId, LaunchGeneration, StatusFile]() mutable
    {
        FPlatformProcess::WaitForProc(Process);
        int32 ExitCode = -1;
        FPlatformProcess::GetProcReturnCode(Process, &ExitCode);
        FPlatformProcess::CloseProc(Process);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, LaunchId, LaunchGeneration, StatusFile, ExitCode]()
        {
            auto* Launcher = WeakThis.Get();
            if (!Launcher || !Launcher->LaunchState.TryComplete(LaunchId, LaunchGeneration)) { return; }
            // 只在完成通知后读取一次终态，固定枚举不会把脚本输出、环境或凭据投影到Widget。
            FString Current;
            const int64 Size = IFileManager::Get().FileSize(*StatusFile);
            if (Size >= 0 && Size <= 64) { FFileHelper::LoadFileToString(Current, *StatusFile); Current.TrimStartAndEndInline(); }
            if (ExitCode == 0 && Current == TEXT("Exited"))
            { Launcher->SetStatus(FText::FromString(TEXT("插件演示进程已退出，可以重新启动；原登录流程未改变。"))); }
            else
            {
                Launcher->SetStatus(FText::FromString(FString::Printf(TEXT("启动检查或演示进程失败，未切换登录模式。查看Saved/Validation/PluginDemos/LoginLaunch/%s；先核对完整Editor构建证据与资源。"),
                    *LaunchId.ToString(EGuidFormats::DigitsWithHyphens))));
            }
        });
    });
}

void UDivineBeastsPluginDemoLauncher::Deinitialize()
{
    LaunchState.Invalidate();
    OnStatusChanged.Clear();
    Super::Deinitialize();
}
