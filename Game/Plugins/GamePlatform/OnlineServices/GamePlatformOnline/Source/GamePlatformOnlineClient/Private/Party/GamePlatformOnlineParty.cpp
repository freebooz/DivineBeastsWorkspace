// 平台客户端普通队伍传输适配；复用Online唯一认证、同源HTTP、请求预算与取消实现，不新增网络执行器。
#include "GamePlatformOnlineClientSubsystem.h"
#include "Party/GamePlatformPartyCodec.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    /** 不传播远端任意message；稳定HTTP类别交给项目本地化，权限仍由服务器判断。 */
    FGamePlatformPartyResult Failure(const FGamePlatformAuthenticatedResponse& Response, bool bWrite)
    {
        FGamePlatformPartyResult Result; Result.HttpStatusCode=Response.HttpStatusCode;
        Result.bOutcomeUnknown=bWrite && Response.bMayHaveReachedServer &&
            (Response.HttpStatusCode==0 || Response.HttpStatusCode>=500);
        if (Response.Error==EGamePlatformAuthError::Cancelled) Result.ErrorCode=TEXT("Party.Cancelled");
        else if (Result.bOutcomeUnknown) Result.ErrorCode=TEXT("Party.OutcomeUnknown");
        else switch (Response.HttpStatusCode)
        {
        case 400: Result.ErrorCode=TEXT("Party.InvalidCommand"); break;
        case 401: Result.ErrorCode=TEXT("Party.Unauthenticated"); break;
        case 403: Result.ErrorCode=TEXT("Party.Forbidden"); break;
        case 404: Result.ErrorCode=TEXT("Party.NotFound"); break;
        case 409: Result.ErrorCode=TEXT("Party.Conflict"); break;
        case 429: Result.ErrorCode=TEXT("Party.RateLimited"); break;
        default: Result.ErrorCode=TEXT("Party.ServiceUnavailable"); break;
        }
        return Result;
    }
}

FGamePlatformOnlineRequestHandle UGamePlatformOnlineClientSubsystem::ObserveParty(const FString& Cursor,
    const FGamePlatformOnlineRequestOptions& Options,TFunction<void(const FGamePlatformPartyResult&)> Completion)
{
    // POST观察无业务写副作用；每次传输身份只用于现有安全重放门禁，不创建队伍或匹配票据。
    FGamePlatformAuthenticatedRequest Request;Request.Verb=TEXT("POST");Request.RelativePath=TEXT("/v1/party/observe");
    Request.bIdempotent=true;Request.IdempotencyKey=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TSharedRef<FJsonObject> Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("cursor"),Cursor);
    Body->SetNumberField(TEXT("waitMilliseconds"),Cursor.IsEmpty()?0:8000);
    FJsonSerializer::Serialize(Body,TJsonWriterFactory<>::Create(&Request.Body));
    return SendAuthenticatedRequest(MoveTemp(Request),Options,[Done=MoveTemp(Completion)](FGamePlatformAuthenticatedResponse Response)
    {
        FGamePlatformPartyResult Result;
        if (!Response.IsSuccess()) Result=Failure(Response,false);
        else if (!GamePlatformPartyCodec::DecodeObservation(Response.Body,Result)) Result.ErrorCode=TEXT("Party.InvalidResponse");
        Result.HttpStatusCode=Response.HttpStatusCode;if(Done)Done(Result);
    });
}

FGamePlatformOnlineRequestHandle UGamePlatformOnlineClientSubsystem::GetCurrentParty(
    const FGamePlatformOnlineRequestOptions& Options,TFunction<void(const FGamePlatformPartyResult&)> Completion)
{
    FGamePlatformAuthenticatedRequest Request; Request.Verb=TEXT("GET"); Request.RelativePath=TEXT("/v1/party/current"); Request.bIdempotent=true;
    return SendAuthenticatedRequest(MoveTemp(Request),Options,[Done=MoveTemp(Completion)](FGamePlatformAuthenticatedResponse Response)
    {
        FGamePlatformPartyResult Result;
        if (!Response.IsSuccess()) Result=Failure(Response,false);
        else if (!(Result.bSuccess=GamePlatformPartyCodec::DecodeSnapshot(Response.Body,Result.Snapshot))) Result.ErrorCode=TEXT("Party.InvalidResponse");
        Result.HttpStatusCode=Response.HttpStatusCode; if(Done)Done(Result);
    });
}
FGamePlatformOnlineRequestHandle UGamePlatformOnlineClientSubsystem::GetPartyInvitations(
    const FGamePlatformOnlineRequestOptions& Options,TFunction<void(const FGamePlatformPartyResult&)> Completion)
{
    FGamePlatformAuthenticatedRequest Request; Request.Verb=TEXT("GET"); Request.RelativePath=TEXT("/v1/party/invitations"); Request.bIdempotent=true;
    return SendAuthenticatedRequest(MoveTemp(Request),Options,[Done=MoveTemp(Completion)](FGamePlatformAuthenticatedResponse Response)
    {
        FGamePlatformPartyResult Result;
        if (!Response.IsSuccess()) Result=Failure(Response,false);
        else if (!(Result.bSuccess=GamePlatformPartyCodec::DecodeInvitations(Response.Body,Result.Invitations))) Result.ErrorCode=TEXT("Party.InvalidResponse");
        Result.HttpStatusCode=Response.HttpStatusCode; if(Done)Done(Result);
    });
}
FGamePlatformOnlineRequestHandle UGamePlatformOnlineClientSubsystem::CommandParty(const FGamePlatformPartyCommand& Command,
    const FGamePlatformOnlineRequestOptions& Options,TFunction<void(const FGamePlatformPartyResult&)> Completion)
{
    FString Body;
    if (!GamePlatformPartyCodec::EncodeCommand(Command,Body))
    {
        // 参数失败也不内联调用：调用者先登记等待身份后再收到终态，避免同步回调覆盖后继状态。
        AsyncTask(ENamedThreads::GameThread,[Done=MoveTemp(Completion)]()
        { FGamePlatformPartyResult Result; Result.ErrorCode=TEXT("Party.InvalidCommand"); if(Done)Done(Result); });
        return {};
    }
    FGamePlatformAuthenticatedRequest Request; Request.Verb=TEXT("POST"); Request.RelativePath=TEXT("/v1/party/commands");
    Request.Body=MoveTemp(Body); Request.IdempotencyKey=Command.ClientRequestId; Request.bIdempotent=true;
    return SendAuthenticatedRequest(MoveTemp(Request),Options,[Command,Done=MoveTemp(Completion)](FGamePlatformAuthenticatedResponse Response)
    {
        FGamePlatformPartyResult Result;
        if (!Response.IsSuccess()) Result=Failure(Response,true);
        else if (!(Result.bSuccess=GamePlatformPartyCodec::DecodeCommandResult(Response.Body,Command,Result.Snapshot)))
        { Result.ErrorCode=TEXT("Party.InvalidResponse"); Result.bOutcomeUnknown=true; }
        Result.HttpStatusCode=Response.HttpStatusCode; if(Done)Done(Result);
    });
}
