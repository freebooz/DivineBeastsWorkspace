#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GamePlatformOnlineClientSubsystem.generated.h"

/** EGamePlatformAuthState（平台认证状态）。 */
UENUM(BlueprintType)
enum class EGamePlatformAuthState : uint8
{
    LoggedOut,
    LoggingIn,
    Authenticated,
    Refreshing,
    LoggingOut,
    Failed
};

/** EGamePlatformAuthError（平台认证错误）。 */
UENUM(BlueprintType)
enum class EGamePlatformAuthError : uint8
{
    None,
    ProviderUnavailable,
    InvalidCredentials,
    AccountLocked,
    Maintenance,
    NetworkUnavailable,
    AuthExpired,
    ContractIncompatible,
    Cancelled,
    TimedOut,
    Unknown
};

/** FGamePlatformAuthSnapshot（平台认证公开快照，不含密码/Token）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMONLINECLIENT_API FGamePlatformAuthSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EGamePlatformAuthState State = EGamePlatformAuthState::LoggedOut;
    UPROPERTY(BlueprintReadOnly) EGamePlatformAuthError Error = EGamePlatformAuthError::None;
    UPROPERTY(BlueprintReadOnly) FString AccountId;
    UPROPERTY(BlueprintReadOnly) FGuid AuthGeneration;
};

using FGamePlatformAuthCompletion = TFunction<void(
    bool,
    const FString&,
    EGamePlatformAuthError)>;

/** IGamePlatformOnlineAuthProvider（平台认证提供者接口）。 */
class GAMEPLATFORMONLINECLIENT_API IGamePlatformOnlineAuthProvider
{
public:
    virtual ~IGamePlatformOnlineAuthProvider() = default;

    virtual void TryAutoLogin(FGamePlatformAuthCompletion Completion) = 0;
    virtual void LoginWithCredentials(
        const FString& LoginName,
        const FString& Password,
        FGamePlatformAuthCompletion Completion) = 0;
    virtual void Refresh(FGamePlatformAuthCompletion Completion) = 0;
    virtual void Logout(TFunction<void()> Completion) = 0;

    /** 只在发起受保护请求的瞬间读取，不允许调用方持久化或日志输出。 */
    virtual FString GetAuthorizationHeaderValue() const = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformAuthSnapshotChangedNative,
    const FGamePlatformAuthSnapshot&);

/**
 * UGamePlatformOnlineClientSubsystem（平台在线客户端子系统）。
 * 认证协议由Provider实现；本子系统只管理公开AuthState和生命周期。
 */
UCLASS()
class GAMEPLATFORMONLINECLIENT_API UGamePlatformOnlineClientSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    void SetProvider(TSharedPtr<IGamePlatformOnlineAuthProvider> InProvider);
    void TryAutoLogin();
    void LoginWithCredentials(const FString& LoginName, const FString& Password);
    void Refresh();
    void Logout();

    FGamePlatformAuthSnapshot GetSnapshot() const { return Snapshot; }
    FString GetAuthorizationHeaderValueTransient() const;

    FGamePlatformAuthSnapshotChangedNative& OnAuthStateChanged()
    {
        return AuthStateChanged;
    }

private:
    void BeginAuth(EGamePlatformAuthState State);
    void CompleteAuth(
        const FGuid& Generation,
        bool bSuccess,
        const FString& AccountId,
        EGamePlatformAuthError Error);

    TSharedPtr<IGamePlatformOnlineAuthProvider> Provider;
    FGamePlatformAuthSnapshot Snapshot;
    FGamePlatformAuthSnapshotChangedNative AuthStateChanged;
};
