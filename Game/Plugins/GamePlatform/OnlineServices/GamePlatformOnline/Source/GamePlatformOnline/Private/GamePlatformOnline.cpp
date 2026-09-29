#include "Interfaces/IGamePlatformOnlineService.h"

#include "Engine/GameInstance.h"
#include "Modules/ModuleManager.h"

namespace
{
TMap<TWeakObjectPtr<UGameInstance>, IGamePlatformOnlineService*> GInstanceServices;

bool IsAsciiIdentity(const FString& Value, int32 MaxChars)
{
    if (Value.IsEmpty() || Value.Len() > MaxChars)
    {
        return false;
    }
    for (TCHAR C : Value)
    {
        if (C < 33 || C > 126)
        {
            return false;
        }
    }
    return true;
}

bool ParsePort(const FString& Text)
{
    if (Text.IsEmpty() || Text.Len() > 5)
    {
        return false;
    }
    int32 Port = 0;
    for (TCHAR C : Text)
    {
        if (C < TEXT('0') || C > TEXT('9'))
        {
            return false;
        }
        Port = Port * 10 + static_cast<int32>(C - TEXT('0'));
    }
    return Port >= 1 && Port <= 65535;
}

bool IsOriginValid(const FGamePlatformOnlineConfiguration& Configuration)
{
    const FString Origin = Configuration.ServiceOrigin.TrimStartAndEnd();
    const bool bHttps = Origin.StartsWith(TEXT("https://"));
    const bool bHttp = Origin.StartsWith(TEXT("http://"));
#if UE_BUILD_SHIPPING
    constexpr bool bShipping = true;
#else
    constexpr bool bShipping = false;
#endif

    if ((!bHttps && !bHttp) ||
        Origin.Len() > 2048 ||
        (!bHttps &&
         (!Configuration.bAllowLoopbackHttpDevelopment || bShipping)))
    {
        return false;
    }

    FString Authority = Origin.Mid(bHttps ? 8 : 7);
    if (Authority.IsEmpty() ||
        Authority.Contains(TEXT("/")) ||
        Authority.Contains(TEXT("@")) ||
        Authority.Contains(TEXT("?")) ||
        Authority.Contains(TEXT("#")) ||
        Authority.Contains(TEXT("\\")) ||
        Authority.Contains(TEXT("%")) ||
        Authority.Contains(TEXT(" ")) ||
        Authority.Contains(TEXT("\t")) ||
        Authority.Contains(TEXT("\r")) ||
        Authority.Contains(TEXT("\n")))
    {
        return false;
    }

    FString Host;
    FString Port;
    if (Authority.StartsWith(TEXT("[")))
    {
        if (!Authority.StartsWith(TEXT("[::1]")))
        {
            return false;
        }
        Host = TEXT("[::1]");
        if (Authority.Len() > 5)
        {
            if (Authority[5] != TEXT(':'))
            {
                return false;
            }
            Port = Authority.Mid(6);
            if (!ParsePort(Port))
            {
                return false;
            }
        }
    }
    else
    {
        int32 Colon = INDEX_NONE;
        if (Authority.FindChar(TEXT(':'), Colon))
        {
            Host = Authority.Left(Colon);
            Port = Authority.Mid(Colon + 1);
            if (!ParsePort(Port))
            {
                return false;
            }
        }
        else
        {
            Host = Authority;
        }

        if (Host.IsEmpty() ||
            Host.StartsWith(TEXT(".")) ||
            Host.EndsWith(TEXT(".")))
        {
            return false;
        }

        TArray<FString> Labels;
        Host.ParseIntoArray(Labels, TEXT("."), false);
        if (Labels.IsEmpty())
        {
            return false;
        }
        for (const FString& Label : Labels)
        {
            if (Label.IsEmpty() ||
                Label.Len() > 63 ||
                Label.StartsWith(TEXT("-")) ||
                Label.EndsWith(TEXT("-")))
            {
                return false;
            }
            for (TCHAR C : Label)
            {
                if (!((C >= TEXT('a') && C <= TEXT('z')) ||
                      (C >= TEXT('0') && C <= TEXT('9')) ||
                      C == TEXT('-')))
                {
                    return false;
                }
            }
        }
    }

    return bHttps ||
        Host == TEXT("127.0.0.1") ||
        Host == TEXT("[::1]");
}

bool IsPositiveFinite(double Value)
{
    return FMath::IsFinite(Value) && Value > 0.0;
}

void PruneDeadInstanceServices()
{
    for (auto It = GInstanceServices.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || It.Value() == nullptr)
        {
            It.RemoveCurrent();
        }
    }
}
}

IGamePlatformOnlineService* IGamePlatformOnlineService::Get(
    UGameInstance& GameInstance)
{
    check(IsInGameThread());
    PruneDeadInstanceServices();
    if (IGamePlatformOnlineService** Found =
            GInstanceServices.Find(&GameInstance))
    {
        return *Found;
    }
    return nullptr;
}

bool IGamePlatformOnlineService::RegisterInstanceService(
    UGameInstance& GameInstance,
    IGamePlatformOnlineService& Service)
{
    check(IsInGameThread());
    PruneDeadInstanceServices();

    if (IGamePlatformOnlineService** Existing =
            GInstanceServices.Find(&GameInstance))
    {
        return *Existing == &Service;
    }

    GInstanceServices.Add(&GameInstance, &Service);
    return true;
}

void IGamePlatformOnlineService::UnregisterInstanceService(
    UGameInstance& GameInstance,
    IGamePlatformOnlineService& Service)
{
    check(IsInGameThread());
    if (IGamePlatformOnlineService** Existing =
            GInstanceServices.Find(&GameInstance);
        Existing && *Existing == &Service)
    {
        GInstanceServices.Remove(&GameInstance);
    }
    PruneDeadInstanceServices();
}

FGamePlatformResult IGamePlatformOnlineService::ValidateConfiguration(
    const FGamePlatformOnlineConfiguration& C)
{
    if (!C.bVerifyCertificates ||
        !IsOriginValid(C) ||
        !IsAsciiIdentity(C.GameId, 128) ||
        !IsAsciiIdentity(C.ClientVersion, 128) ||
        C.RequiredContractVersion != TEXT("1.0.0") ||
        !IsPositiveFinite(C.RequestDeadlineSeconds) ||
        C.RequestDeadlineSeconds > 300.0 ||
        !IsPositiveFinite(C.AttemptTimeoutSeconds) ||
        C.AttemptTimeoutSeconds > C.RequestDeadlineSeconds ||
        !IsPositiveFinite(C.RevocationDeadlineSeconds) ||
        C.RevocationDeadlineSeconds > 30.0 ||
        !IsPositiveFinite(C.RetryBaseDelaySeconds) ||
        !IsPositiveFinite(C.MaxRetryDelaySeconds) ||
        C.MaxRetryDelaySeconds > C.RequestDeadlineSeconds ||
        C.RetryBaseDelaySeconds > C.MaxRetryDelaySeconds ||
        C.MaxConcurrentRequests < 1 ||
        C.MaxConcurrentRequests > 32 ||
        C.MaxQueuedRequests < 1 ||
        C.MaxQueuedRequests > 1024 ||
        C.MaxRefreshWaiters < 1 ||
        C.MaxRefreshWaiters > 1024 ||
        C.MaxResponseBytes < 256 ||
        C.MaxResponseBytes > 4 * 1024 * 1024 ||
        C.MaxRequestBytes < 256 ||
        C.MaxRequestBytes > 64 * 1024 ||
        C.MaxReadRetries < 0 ||
        C.MaxReadRetries > 5)
    {
        return FGamePlatformResult::Failure(
            TEXT("OnlineConfigurationInvalid"),
            TEXT("在线配置不满足地址、安全、并发、容量或超时约束。"));
    }

    return FGamePlatformResult::Success();
}

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformOnline)

