#include "Bootstrap/Online/DBAFoundationOnlineContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAOnlineExercise, Log, All);

bool UDBAFoundationOnlineContext::TickExercise(float)
{
    if (!bConfigured || Exercise == EExercise::Completed || Exercise == EExercise::Failed)
    {
        ExerciseTicker.Reset();
        return false;
    }
    if (ExerciseRequest.RequestId.IsValid()) { return true; }
    auto* Online = Service();
    if (!Online) { FailExercise(FGamePlatformResult::Failure(TEXT("OnlineServiceExpired"), TEXT("练习期间服务已关闭"))); return true; }
    const uint64 ExpectedGeneration = Generation;
    const EExercise ExpectedStep = Exercise;
    TWeakObjectPtr<UDBAFoundationOnlineContext> WeakThis(this);
    // 每次操作身份/步骤都必须匹配；终态只推进下一Tick，不在完成栈递归提交网络。
    auto Accept = [WeakThis, ExpectedGeneration, ExpectedStep](const FGamePlatformOnlineResult& Result) -> UDBAFoundationOnlineContext*
    {
        auto* Self = WeakThis.Get();
        if (!Self || Self->Generation != ExpectedGeneration || Self->Exercise != ExpectedStep) { return nullptr; }
        Self->ExerciseRequest = {};
        Self->Observe(Result);
        if (!Result.Result.IsSuccess()) { Self->FailExercise(Result.Result); return nullptr; }
        return Self;
    };
    switch (Exercise)
    {
    case EExercise::Update:
    {
        LastOperation = TEXT("UpdateProfileA");
        const auto Profile = Online->GetCachedProfile();
        if (!Profile.IsSet() || Profile->PlayerId != PlayerA)
        {
            FailExercise(FGamePlatformResult::Failure(TEXT("ProfileNotReady"), TEXT("不能对未读取或错账号资料执行更新")));
            break;
        }
        const int64 PreviousRevision = Profile->Revision;
        FGamePlatformOnlineProfileUpdateRequest Request;
        Request.DisplayName = ExpectedDisplayName;
        Request.ExpectedRevision = PreviousRevision;
        Request.IdempotencyKey = TEXT("foundation-") + RunId + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ExerciseRequest = Online->UpdateCurrentPlayerProfile(Request, Options(), [Accept, PreviousRevision](const auto& Result)
        {
            if (auto* Self = Accept(Result))
            {
                if (Result.Profile.PlayerId != Self->PlayerA || Result.Profile.DisplayName != Self->ExpectedDisplayName ||
                    Result.Profile.Revision <= PreviousRevision)
                { Self->FailExercise(FGamePlatformResult::Failure(TEXT("ProfileWriteNotConfirmed"), TEXT("资料更新响应未确认本人新值及递增修订"))); return; }
                Self->UpdatedRevision = Result.Profile.Revision;
                Self->Exercise = EExercise::Refresh;
            }
        });
        break;
    }
    case EExercise::Refresh:
    {
        LastOperation = TEXT("RefreshA");
        const auto Before = Online->GetAuthentication();
        ExerciseRequest = Online->RefreshAuthentication(Options(), [Accept, Before](const auto& Result)
        {
            if (auto* Self = Accept(Result))
            {
                if (Result.Authentication.PlayerId != Self->PlayerA || Result.Authentication.AuthContextId != Before.AuthContextId ||
                    Result.Authentication.TokenVersion <= Before.TokenVersion)
                { Self->FailExercise(FGamePlatformResult::Failure(TEXT("RefreshNotConfirmed"), TEXT("刷新未保持主体与上下文或未增加令牌版本"))); return; }
                Self->Exercise = EExercise::LogoutA;
            }
        });
        break;
    }
    case EExercise::LogoutA:
    case EExercise::LogoutForSwitch:
    case EExercise::LogoutB:
        LastOperation = TEXT("Logout");
        ExerciseRequest = Online->Logout([Accept, ExpectedStep](const auto& Result)
        {
            if (auto* Self = Accept(Result))
            {
                if (Result.Disposition != EGamePlatformOnlineLogoutDisposition::ServerRevoked ||
                    !Self->Service() || Self->Service()->GetAuthentication().State != EGamePlatformOnlineAuthState::SignedOut ||
                    Self->Service()->GetCachedProfile().IsSet())
                { Self->FailExercise(FGamePlatformResult::Failure(TEXT("LogoutNotConfirmed"), TEXT("远端撤销或本地资料清理未确认"))); return; }
                Self->Exercise = ExpectedStep == EExercise::LogoutA ? EExercise::LoginA :
                    ExpectedStep == EExercise::LogoutForSwitch ? EExercise::LoginB : EExercise::Completed;
                if (Self->Exercise == EExercise::Completed)
                {
                    // 本标记只证明客户端练习；不能冒充后端进程重启、旧原始令牌拒绝或多PIE验证。
                    UE_LOG(LogDBAOnlineExercise, Display, TEXT("FoundationOnlineVerified RunId=%s ProfileRevision=%lld"),
                        *Self->RunId, Self->UpdatedRevision);
                    Self->PasswordA.Reset(); Self->PasswordB.Reset();
                }
            }
        });
        break;
    case EExercise::LoginA:
    case EExercise::LoginB:
    {
        const bool bB = Exercise == EExercise::LoginB;
        LastOperation = bB ? TEXT("LoginB") : TEXT("ReloginA");
        ExerciseRequest = Online->Login(LoginRequest(bB), Options(), [Accept, bB](const auto& Result)
        {
            if (auto* Self = Accept(Result))
            {
                if (Result.Authentication.PlayerId != (bB ? Self->PlayerB : Self->PlayerA) ||
                    !Self->Service() || Self->Service()->GetCachedProfile().IsSet())
                { Self->FailExercise(FGamePlatformResult::Failure(TEXT("AccountIsolationFailed"), TEXT("登录主体不匹配或泄漏上个账号缓存"))); return; }
                Self->Exercise = bB ? EExercise::ReadB : EExercise::ReadA;
            }
        });
        break;
    }
    case EExercise::ReadA:
    case EExercise::ReadB:
    {
        const bool bB = Exercise == EExercise::ReadB;
        LastOperation = bB ? TEXT("ReadProfileB") : TEXT("ReReadProfileA");
        ExerciseRequest = Online->GetCurrentPlayerProfile(Options(), [Accept, bB](const auto& Result)
        {
            if (auto* Self = Accept(Result))
            {
                if (Result.Profile.PlayerId != (bB ? Self->PlayerB : Self->PlayerA) ||
                    (!bB && (Result.Profile.DisplayName != Self->ExpectedDisplayName || Result.Profile.Revision < Self->UpdatedRevision)))
                { Self->FailExercise(FGamePlatformResult::Failure(TEXT("ProfileIsolationOrPersistenceFailed"), TEXT("真实资料主体、更新值或修订不符合预期"))); return; }
                Self->Exercise = bB ? EExercise::LogoutB : EExercise::LogoutForSwitch;
            }
        });
        break;
    }
    default:
        FailExercise(FGamePlatformResult::Failure(TEXT("ExerciseStateInvalid"), TEXT("在线练习阶段非法")));
        break;
    }
    return true;
}
