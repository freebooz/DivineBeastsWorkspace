#include "Bootstrap/Online/DBAFoundationOnlineContext.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAFoundationOnline, Log, All);

IGamePlatformOnlineService* UDBAFoundationOnlineContext::Service() const
{
    return Instance.IsValid() ? IGamePlatformOnlineService::Get(*Instance.Get()) : nullptr;
}

FGamePlatformOnlineRequestOptions UDBAFoundationOnlineContext::Options() const
{
    FGamePlatformOnlineRequestOptions Value;
    Value.Owner = const_cast<UDBAFoundationOnlineContext*>(this);
    return Value; // 实例级请求允许进入测试地图后继续；本项目上下文负责关闭。
}

FGamePlatformOnlineLoginRequest UDBAFoundationOnlineContext::LoginRequest(bool bAccountB) const
{
    FGamePlatformOnlineLoginRequest Value;
    Value.AccountName = bAccountB ? AccountB : AccountA;
    Value.Credential = bAccountB ? PasswordB : PasswordA;
    Value.DeviceId = TEXT("foundation-online-") + RunId;
    return Value;
}

FGamePlatformResult UDBAFoundationOnlineContext::Initialize(UGameInstance& Owner, const FString& InRunId)
{
    check(IsInGameThread());
    if (bConfigured) { return FGamePlatformResult::Failure(TEXT("OnlineAlreadyConfigured"), TEXT("在线开发上下文已初始化")); }
    if (UE_BUILD_SHIPPING || IsRunningCommandlet() || IsRunningDedicatedServer() ||
        !FParse::Param(FCommandLine::Get(), TEXT("FoundationOnlineIntegration")) ||
        FParse::Param(FCommandLine::Get(), TEXT("FoundationStandalone")))
    {
        return FGamePlatformResult::Failure(TEXT("OnlineEntryDisabled"), TEXT("在线与单机入口必须明确互斥；当前上下文禁止玩家在线联调"));
    }
    FGuid ParsedRun;
    if (!FGuid::Parse(InRunId, ParsedRun) || !ParsedRun.IsValid())
    {
        return FGamePlatformResult::Failure(TEXT("OnlineRunIdInvalid"), TEXT("缺少本次联调身份"));
    }
    Instance = &Owner;
    RunId = ParsedRun.ToString(EGuidFormats::DigitsWithHyphensLower);
    auto* Online = Service();
    if (!Online) { return FGamePlatformResult::Failure(TEXT("OnlineServiceMissing"), TEXT("当前实例在线服务不可用")); }
    FGamePlatformOnlineConfiguration Config;
    if (!FParse::Value(FCommandLine::Get(), TEXT("FoundationOnlineGateway="), Config.ServiceOrigin))
    {
        return FGamePlatformResult::Failure(TEXT("OnlineGatewayMissing"), TEXT("必须显式提供本次网关地址，不读取生产默认地址"));
    }
    Config.GameId = TEXT("divine-beasts");
    Config.ClientVersion = TEXT("0.1.0");
    Config.bAllowLoopbackHttpDevelopment = true; // 插件仍严格限制字面回环；不放宽TLS或私网。
    LastResult = Online->Configure(Config);
    if (!LastResult.IsSuccess()) { return LastResult; }

    // 文件路径由受控子进程环境注入；脚本已验证ACL。这里再限制本RunId秘密目录、尺寸及结构。
    FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("DBA_ONLINE_CREDENTIALS_FILE"));
    const FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Saved/Validation/GamePlatformOnline") / RunId / TEXT("Secrets"));
    Path = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Path);
    const int64 Size = IFileManager::Get().FileSize(*Path);
    if (!FPaths::IsUnderDirectory(Path, Allowed) || Size < 1 || Size > 16384)
    {
        return FGamePlatformResult::Failure(TEXT("OnlineCredentialInputInvalid"), TEXT("仅允许本次运行的受控短期凭据文件，或文件尺寸非法"));
    }
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
    {
        return FGamePlatformResult::Failure(TEXT("OnlineCredentialInputInvalid"), TEXT("受控凭据文件不可读或格式非法"));
    }
    Text.Reset(); // 清除本对象逻辑引用；不声称普通字符串释放等于安全擦除。
    FString FileRun, Game;
    const TSharedPtr<FJsonObject>* Accounts = nullptr;
    const TSharedPtr<FJsonObject>* A = nullptr;
    const TSharedPtr<FJsonObject>* B = nullptr;
    const bool bValid = Root->TryGetStringField(TEXT("runId"), FileRun) && FileRun == RunId &&
        Root->TryGetStringField(TEXT("gameId"), Game) && Game == Config.GameId &&
        Root->TryGetObjectField(TEXT("accounts"), Accounts) && (*Accounts)->TryGetObjectField(TEXT("A"), A) &&
        (*Accounts)->TryGetObjectField(TEXT("B"), B) &&
        (*A)->TryGetStringField(TEXT("accountName"), AccountA) && (*A)->TryGetStringField(TEXT("password"), PasswordA) &&
        (*A)->TryGetStringField(TEXT("playerId"), PlayerA) &&
        (*B)->TryGetStringField(TEXT("accountName"), AccountB) && (*B)->TryGetStringField(TEXT("password"), PasswordB) &&
        (*B)->TryGetStringField(TEXT("playerId"), PlayerB) && !AccountA.IsEmpty() && !AccountB.IsEmpty() &&
        !PasswordA.IsEmpty() && !PasswordB.IsEmpty() && !PlayerA.IsEmpty() && !PlayerB.IsEmpty() &&
        AccountA != AccountB && PlayerA != PlayerB;
    Root.Reset();
    if (!bValid)
    {
        AccountA.Reset(); PasswordA.Reset(); PlayerA.Reset(); AccountB.Reset(); PasswordB.Reset(); PlayerB.Reset();
        return FGamePlatformResult::Failure(TEXT("OnlineCredentialInputInvalid"), TEXT("凭据不属于本次运行或隔离账号字段不完整"));
    }
    bConfigured = true;
    bExerciseRequested = FParse::Param(FCommandLine::Get(), TEXT("FoundationOnlineExercise"));
    ++Generation;
    LastResult = FGamePlatformResult::Success();
    return LastResult;
}

void UDBAFoundationOnlineContext::Observe(const FGamePlatformOnlineResult& Result)
{
    LastResult = Result.Result;
    LastElapsedSeconds = Result.ElapsedSeconds;
    UE_LOG(LogDBAFoundationOnline, Display, TEXT("FoundationOnlineOperation RunId=%s RequestId=%s Operation=%s Success=%d Code=%s"),
        *RunId, *Result.Request.RequestId.ToString(), *LastOperation, Result.Result.IsSuccess() ? 1 : 0, *Result.Result.Code.ToString());
}

FGamePlatformOnlineRequestHandle UDBAFoundationOnlineContext::Begin(EDBAOnlineOperation Operation,
    TFunction<void(FGamePlatformResult)> Completion)
{
    check(IsInGameThread());
    auto* Online = Service();
    if (!bConfigured || !Online)
    {
        Completion(FGamePlatformResult::Failure(TEXT("OnlineServiceMissing"), TEXT("在线开发上下文尚未配置")));
        return {};
    }
    const uint64 Expected = Generation;
    TWeakObjectPtr<UDBAFoundationOnlineContext> WeakThis(this);
    if (Operation == EDBAOnlineOperation::ProbeService)
    {
        LastOperation = TEXT("ProbeService");
        return Online->ProbeService(Options(), [WeakThis, Expected, Done = MoveTemp(Completion)](const auto& Result) mutable
        { if (auto* Self = WeakThis.Get(); Self && Self->Generation == Expected) { Self->Observe(Result); Done(Result.Result); } });
    }
    if (Operation == EDBAOnlineOperation::Login)
    {
        LastOperation = TEXT("LoginA");
        return Online->Login(LoginRequest(false), Options(), [WeakThis, Expected, Done = MoveTemp(Completion)](const auto& Result) mutable
        {
            if (auto* Self = WeakThis.Get(); Self && Self->Generation == Expected)
            {
                Self->Observe(Result);
                Done(Result.Result.IsSuccess() && Result.Authentication.PlayerId != Self->PlayerA ?
                    FGamePlatformResult::Failure(TEXT("OnlineAccountMismatch"), TEXT("真实认证主体与本次隔离账号不一致")) : Result.Result);
            }
        });
    }
    LastOperation = TEXT("ReadProfileA");
    return Online->GetCurrentPlayerProfile(Options(), [WeakThis, Expected, Done = MoveTemp(Completion)](const auto& Result) mutable
    {
        if (auto* Self = WeakThis.Get(); Self && Self->Generation == Expected)
        {
            Self->Observe(Result);
            Done(Result.Result.IsSuccess() && Result.Profile.PlayerId != Self->PlayerA ?
                FGamePlatformResult::Failure(TEXT("OnlineProfileMismatch"), TEXT("返回资料不属于本次已认证隔离账号")) : Result.Result);
        }
    });
}

bool UDBAFoundationOnlineContext::IsAuthenticatedWithProfile() const
{
    auto* Online = Service();
    if (!bConfigured || !Online) { return false; }
    const auto Auth = Online->GetAuthentication();
    const auto Profile = Online->GetCachedProfile();
    return Auth.State == EGamePlatformOnlineAuthState::SignedIn && Profile.IsSet() &&
        Auth.PlayerId == PlayerA && Profile->PlayerId == Auth.PlayerId && Profile->Revision >= 0;
}

FString UDBAFoundationOnlineContext::GetDiagnostics() const
{
    auto* Online = Service();
    if (!Online) { return TEXT("在线服务未初始化"); }
    const auto Auth = Online->GetAuthentication();
    const auto Profile = Online->GetCachedProfile();
    const FString Masked = Auth.PlayerId.IsEmpty() ? TEXT("未登录") : TEXT("…") + Auth.PlayerId.Right(6);
    return FString::Printf(TEXT("在线开发验证（非正式世界）\n认证状态：%d / 玩家：%s\n显示名：%s / 修订：%lld\n操作：%s / 耗时：%.3f秒\n结果：%s %s"),
        static_cast<int32>(Auth.State), *Masked, Profile.IsSet() ? *Profile->DisplayName : TEXT("未读取"),
        Profile.IsSet() ? Profile->Revision : -1, *LastOperation, LastElapsedSeconds, *LastResult.Code.ToString(), *LastResult.Message);
}

void UDBAFoundationOnlineContext::OnFoundationReady()
{
    if (!IsAuthenticatedWithProfile()) { return; }
    UE_LOG(LogDBAFoundationOnline, Display, TEXT("FoundationOnlineReady RunId=%s 在线认证已验证，当前仍为基础测试场景"), *RunId);
    if (bExerciseRequested && Exercise == EExercise::Idle)
    {
        ExpectedDisplayName = TEXT("在线验证_") + RunId.Left(8);
        Exercise = EExercise::Update;
        ExerciseTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,
            &UDBAFoundationOnlineContext::TickExercise), 0.1f);
    }
}

void UDBAFoundationOnlineContext::FailExercise(const FGamePlatformResult& Result)
{
    LastResult = Result;
    Exercise = EExercise::Failed;
    UE_LOG(LogDBAFoundationOnline, Warning, TEXT("FoundationOnlineFailed RunId=%s Operation=%s Code=%s"),
        *RunId, *LastOperation, *Result.Code.ToString());
}

void UDBAFoundationOnlineContext::Shutdown()
{
    ++Generation; // 先使练习与项目节点回调失效，再取消底层可能同步完成的请求。
    bConfigured = false;
    if (ExerciseTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(ExerciseTicker); ExerciseTicker.Reset(); }
    if (auto* Online = Service())
    {
        if (ExerciseRequest.RequestId.IsValid()) { Online->Cancel(ExerciseRequest); }
        // 游戏实例马上关闭时不等待网络，保留插件的本地清理/远端未确认语义，不声称已撤销。
        Online->Logout([](const FGamePlatformOnlineLogoutResult&) {});
    }
    ExerciseRequest = {};
    AccountA.Reset(); PasswordA.Reset(); PlayerA.Reset(); AccountB.Reset(); PasswordB.Reset(); PlayerB.Reset();
    Instance.Reset();
}
