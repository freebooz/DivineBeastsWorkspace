#include "Persistence/GamePlatformSettingsClientPersistenceProvider.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/SecureHash.h"
#include "Profiles/GamePlatformUserSettingsProfile.h"
#include "Settings/GamePlatformSettingsProjectSettings.h"

namespace
{
    FString GetProfileSlotBaseName()
    {
        const UGamePlatformSettingsProjectSettings* Settings =
            GetDefault<UGamePlatformSettingsProjectSettings>();
        return Settings && !Settings->LocalProfileSlotName.IsEmpty()
            ? Settings->LocalProfileSlotName
            : TEXT("GamePlatformSettings");
    }

    FString HashUserContextKey(const FString& UserContextKey)
    {
        const FTCHARToUTF8 Utf8(*UserContextKey);
        uint8 Digest[FSHA1::DigestSize] = {};
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
        return BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
    }

    bool IsPersistentClientEnvironment()
    {
        if (IsRunningDedicatedServer() || IsRunningCommandlet())
        {
            return false;
        }
#if WITH_EDITOR
        // 多PIE共享进程级本地存档路径，默认禁止写入真实用户档案。
        if (GIsEditor && !FApp::IsGame())
        {
            return false;
        }
#endif
        return true;
    }
}

FName FGamePlatformSettingsClientPersistenceProvider::GetPersistenceId() const
{
    return FName(TEXT("GamePlatformSettingsClientProfile"));
}

bool FGamePlatformSettingsClientPersistenceProvider::SupportsRuntime(
    const EGamePlatformSettingRuntimeScope RuntimeScope) const
{
    return RuntimeScope == EGamePlatformSettingRuntimeScope::Client;
}

FGamePlatformResult FGamePlatformSettingsClientPersistenceProvider::SetUserContext(
    const FString& UserContextKey)
{
    check(IsInGameThread());

    if (UserContextKey.Len() > 128 ||
        UserContextKey.Contains(TEXT("\r")) ||
        UserContextKey.Contains(TEXT("\n")))
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsUserContextInvalid"),
            TEXT("用户上下文键必须是不超过128字符且不含换行的稳定不透明值。"));
    }

    CurrentUserContextKey = UserContextKey;
    return FGamePlatformResult::Success();
}

FString FGamePlatformSettingsClientPersistenceProvider::GetScopedProfileSlotName() const
{
    const FString BaseName = GetProfileSlotBaseName();
    return CurrentUserContextKey.IsEmpty()
        ? FString()
        : BaseName + TEXT("_") + HashUserContextKey(CurrentUserContextKey);
}

FGamePlatformResult FGamePlatformSettingsClientPersistenceProvider::Load(
    const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
    FGamePlatformSettingsPersistencePayload& OutPayload)
{
    check(IsInGameThread());

    const UGamePlatformSettingsProjectSettings* ProjectSettings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    OutPayload.SchemaVersion =
        ProjectSettings ? ProjectSettings->SchemaVersion : 1;

    if (!IsPersistentClientEnvironment())
    {
        // PIE/命令行只使用默认与Session层，不读取/污染真实用户档案。
        return FGamePlatformResult::Success();
    }

    const FString SlotName = GetScopedProfileSlotName();
    if (SlotName.IsEmpty())
    {
        // Logout/未登录时不读取任何用户档案，只保留默认与Session层。
        return FGamePlatformResult::Success();
    }
    constexpr int32 UserIndex = 0;
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        return FGamePlatformResult::Success();
    }

    USaveGame* Loaded =
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
    UGamePlatformUserSettingsProfile* Profile =
        Cast<UGamePlatformUserSettingsProfile>(Loaded);
    if (!Profile || Profile->SchemaVersion < 1)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsProfileCorrupt"),
            TEXT("本地用户设置档案损坏或类型不兼容；运行时将回退默认值且不覆盖原文件。"));
    }

    OutPayload.SchemaVersion = Profile->SchemaVersion;
    OutPayload.Layers.FindOrAdd(EGamePlatformSettingLayer::User) =
        Profile->UserValues;
    return FGamePlatformResult::Success();
}

FGamePlatformResult
FGamePlatformSettingsClientPersistenceProvider::BeginSave(
    const TMap<FName, FGamePlatformSettingValue>& UserValues,
    const int32 SchemaVersion,
    FGamePlatformSettingsSaveCompletion Completion)
{
    check(IsInGameThread());

    if (!Completion)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsSaveCompletionMissing"),
            TEXT("异步设置保存必须提供完成回调。"));
    }

    if (!IsPersistentClientEnvironment())
    {
        return FGamePlatformResult::Unsupported(
            TEXT("SettingsPersistenceDisabledInPIE"),
            TEXT("PIE/命令行环境默认禁止写入真实用户设置档案。"));
    }

    UGamePlatformUserSettingsProfile* Profile =
        Cast<UGamePlatformUserSettingsProfile>(
            UGameplayStatics::CreateSaveGameObject(
                UGamePlatformUserSettingsProfile::StaticClass()));
    if (!Profile)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsProfileCreateFailed"),
            TEXT("无法创建本地用户设置档案对象。"));
    }

    Profile->SchemaVersion = SchemaVersion;
    Profile->UserValues = UserValues;

    const FString SlotName = GetScopedProfileSlotName();
    if (SlotName.IsEmpty())
    {
        return FGamePlatformResult::Unsupported(
            TEXT("SettingsUserContextMissing"),
            TEXT("未设置用户上下文时不保存User Profile。"));
    }
    constexpr int32 UserIndex = 0;
    UGameplayStatics::AsyncSaveGameToSlot(
        Profile,
        SlotName,
        UserIndex,
        FAsyncSaveGameToSlotDelegate::CreateLambda(
            [Completion = MoveTemp(Completion)](
                const FString&,
                const int32,
                const bool bSucceeded) mutable
            {
                Completion(
                    bSucceeded
                        ? FGamePlatformResult::Success()
                        : FGamePlatformResult::Failure(
                            TEXT("SettingsProfileSaveFailed"),
                            TEXT("本地用户设置档案异步保存失败。")));
            }));

    return FGamePlatformResult::Success();
}
