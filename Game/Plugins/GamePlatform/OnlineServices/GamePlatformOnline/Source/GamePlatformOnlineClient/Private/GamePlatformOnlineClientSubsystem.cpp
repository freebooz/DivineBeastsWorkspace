#include "GamePlatformOnlineClientSubsystem.h"

namespace
{
constexpr int32 MaxLoginNameChars = 256;
constexpr int32 MaxPasswordChars = 1024;
constexpr int32 MaxAccountIdChars = 512;
constexpr int32 MaxAuthorizationHeaderChars = 8192;
}

void UGamePlatformOnlineClientSubsystem::Deinitialize()
{
    Snapshot.AuthGeneration = FGuid::NewGuid();
    Provider.Reset();
    Snapshot = FGamePlatformAuthSnapshot();
    AuthStateChanged.Clear();
    Super::Deinitialize();
}

void UGamePlatformOnlineClientSubsystem::SetProvider(
    TSharedPtr<IGamePlatformOnlineAuthProvider> InProvider)
{
    if (Provider == InProvider)
    {
        return;
    }

    // 切换Provider时使旧异步回调全部失效，避免旧认证结果污染新Provider状态。
    Snapshot.AuthGeneration = FGuid::NewGuid();
    Snapshot.State = EGamePlatformAuthState::LoggedOut;
    Snapshot.Error = EGamePlatformAuthError::None;
    Snapshot.AccountId.Reset();
    Provider = MoveTemp(InProvider);
    AuthStateChanged.Broadcast(Snapshot);
}

void UGamePlatformOnlineClientSubsystem::BeginAuth(EGamePlatformAuthState State)
{
    Snapshot.State = State;
    Snapshot.Error = EGamePlatformAuthError::None;
    Snapshot.AuthGeneration = FGuid::NewGuid();
    AuthStateChanged.Broadcast(Snapshot);
}

void UGamePlatformOnlineClientSubsystem::CompleteAuth(
    const FGuid& Generation,
    bool bSuccess,
    const FString& AccountId,
    EGamePlatformAuthError Error)
{
    if (Snapshot.AuthGeneration != Generation)
    {
        return;
    }

    if (bSuccess &&
        (AccountId.TrimStartAndEnd().IsEmpty() || AccountId.Len() > MaxAccountIdChars))
    {
        bSuccess = false;
        Error = EGamePlatformAuthError::ContractIncompatible;
    }

    Snapshot.State = bSuccess
        ? EGamePlatformAuthState::Authenticated
        : EGamePlatformAuthState::Failed;
    Snapshot.Error = bSuccess ? EGamePlatformAuthError::None : Error;
    Snapshot.AccountId = bSuccess ? AccountId : FString();
    AuthStateChanged.Broadcast(Snapshot);
}

void UGamePlatformOnlineClientSubsystem::TryAutoLogin()
{
    BeginAuth(EGamePlatformAuthState::LoggingIn);
    const FGuid Generation = Snapshot.AuthGeneration;
    if (!Provider.IsValid())
    {
        CompleteAuth(
            Generation,
            false,
            FString(),
            EGamePlatformAuthError::ProviderUnavailable);
        return;
    }

    Provider->TryAutoLogin(
        [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this), Generation](
            bool bSuccess,
            const FString& AccountId,
            EGamePlatformAuthError Error)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->CompleteAuth(Generation, bSuccess, AccountId, Error);
            }
        });
}

void UGamePlatformOnlineClientSubsystem::LoginWithCredentials(
    const FString& LoginName,
    const FString& Password)
{
    BeginAuth(EGamePlatformAuthState::LoggingIn);
    const FGuid Generation = Snapshot.AuthGeneration;
    if (LoginName.TrimStartAndEnd().IsEmpty() ||
        Password.IsEmpty() ||
        LoginName.Len() > MaxLoginNameChars ||
        Password.Len() > MaxPasswordChars)
    {
        CompleteAuth(
            Generation,
            false,
            FString(),
            EGamePlatformAuthError::InvalidCredentials);
        return;
    }
    if (!Provider.IsValid())
    {
        CompleteAuth(
            Generation,
            false,
            FString(),
            EGamePlatformAuthError::ProviderUnavailable);
        return;
    }

    Provider->LoginWithCredentials(
        LoginName,
        Password,
        [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this), Generation](
            bool bSuccess,
            const FString& AccountId,
            EGamePlatformAuthError Error)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->CompleteAuth(Generation, bSuccess, AccountId, Error);
            }
        });
}

void UGamePlatformOnlineClientSubsystem::Refresh()
{
    if (Snapshot.State != EGamePlatformAuthState::Authenticated)
    {
        BeginAuth(EGamePlatformAuthState::Refreshing);
        const FGuid InvalidGeneration = Snapshot.AuthGeneration;
        CompleteAuth(
            InvalidGeneration,
            false,
            FString(),
            EGamePlatformAuthError::AuthExpired);
        return;
    }
    BeginAuth(EGamePlatformAuthState::Refreshing);
    const FGuid Generation = Snapshot.AuthGeneration;
    if (!Provider.IsValid())
    {
        CompleteAuth(
            Generation,
            false,
            FString(),
            EGamePlatformAuthError::ProviderUnavailable);
        return;
    }

    Provider->Refresh(
        [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this), Generation](
            bool bSuccess,
            const FString& AccountId,
            EGamePlatformAuthError Error)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->CompleteAuth(Generation, bSuccess, AccountId, Error);
            }
        });
}

void UGamePlatformOnlineClientSubsystem::Logout()
{
    Snapshot.AuthGeneration = FGuid::NewGuid();
    Snapshot.State = EGamePlatformAuthState::LoggingOut;
    Snapshot.Error = EGamePlatformAuthError::None;
    AuthStateChanged.Broadcast(Snapshot);

    const FGuid Generation = Snapshot.AuthGeneration;
    auto Finish = [WeakThis = TWeakObjectPtr<UGamePlatformOnlineClientSubsystem>(this), Generation]()
    {
        if (!WeakThis.IsValid() || WeakThis->Snapshot.AuthGeneration != Generation)
        {
            return;
        }
        WeakThis->Snapshot.State = EGamePlatformAuthState::LoggedOut;
        WeakThis->Snapshot.AccountId.Reset();
        WeakThis->Snapshot.Error = EGamePlatformAuthError::None;
        WeakThis->AuthStateChanged.Broadcast(WeakThis->Snapshot);
    };

    if (Provider.IsValid())
    {
        Provider->Logout(MoveTemp(Finish));
    }
    else
    {
        Finish();
    }
}

FString UGamePlatformOnlineClientSubsystem::GetAuthorizationHeaderValueTransient() const
{
    if (!Provider.IsValid() ||
        Snapshot.State != EGamePlatformAuthState::Authenticated)
    {
        return FString();
    }

    FString Header = Provider->GetAuthorizationHeaderValue();
    return Header.Len() <= MaxAuthorizationHeaderChars
        ? MoveTemp(Header)
        : FString();
}
