// 平台客户端设置IO适配：GI独占用户上下文，槽名由不透明键派生；AsyncSave复制值和槽名后不依赖Provider寿命，完成不持有GI。
#include "Persistence/GamePlatformSettingsClientPersistenceProvider.h"

#include "Kismet/GameplayStatics.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
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

TUniquePtr<IGamePlatformSettingsPersistenceProvider> FGamePlatformSettingsClientPersistenceProvider::CreateScopedProvider() const
{
    check(IsInGameThread());
    return MakeUnique<FGamePlatformSettingsClientPersistenceProvider>();
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
    return FGamePlatformResult::Unsupported(TEXT("SettingsAsyncLoadRequired"), TEXT("客户端用户档案必须通过BeginLoad异步读取。"));
}

FGamePlatformResult FGamePlatformSettingsClientPersistenceProvider::BeginLoad(
    const TMap<FName, FGamePlatformSettingDescriptor>&,
    FGamePlatformSettingsLoadCompletion Completion)
{
    check(IsInGameThread());
    if (!Completion) { return FGamePlatformResult::Failure(TEXT("SettingsLoadCompletionMissing"), TEXT("设置读取必须提供完成回调。")); }
    const auto* ProjectSettings = GetDefault<UGamePlatformSettingsProjectSettings>();
    FGamePlatformSettingsPersistencePayload Empty;
    Empty.SchemaVersion = ProjectSettings ? ProjectSettings->SchemaVersion : 1;
    const FString SlotName = GetScopedProfileSlotName();
    if (!IsPersistentClientEnvironment() || SlotName.IsEmpty())
    {
        Completion(MoveTemp(Empty), FGamePlatformResult::Success());
        return FGamePlatformResult::Success();
    }
    // 在游戏线程获取平台模块与保存系统，避免后台线程首次加载模块；IO由UE原生异步接口调度。
    auto* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
    if (!SaveSystem) { return FGamePlatformResult::Failure(TEXT("SettingsSaveSystemUnavailable"), TEXT("平台存档系统不可用，未受理档案读取。")); }
    const auto PlatformUser = FPlatformMisc::GetPlatformUserForUserIndex(0);
    // 复制槽名/版本/完成函数，不捕获Provider裸指针；不存在是空档案，损坏/未知错误必须失败。
    SaveSystem->DoesSaveGameExistAsync(*SlotName, PlatformUser,
        [SlotName, Empty = MoveTemp(Empty), Completion = MoveTemp(Completion)](const FString&, FPlatformUserId, ISaveGameSystem::ESaveExistsResult ExistsResult) mutable
        {
            check(IsInGameThread());
            if (ExistsResult == ISaveGameSystem::ESaveExistsResult::DoesNotExist)
            { Completion(MoveTemp(Empty), FGamePlatformResult::Success()); return; }
            if (ExistsResult != ISaveGameSystem::ESaveExistsResult::OK)
            { Completion({}, FGamePlatformResult::Failure(TEXT("SettingsProfileReadFailed"), TEXT("本地档案存在性查询失败或报告损坏，保留旧只读快照。"))); return; }
            UGameplayStatics::AsyncLoadGameFromSlot(SlotName, 0, FAsyncLoadGameFromSlotDelegate::CreateLambda(
                [Completion = MoveTemp(Completion)](const FString&, const int32, USaveGame* Loaded) mutable
                {
                    const auto* Profile = Cast<UGamePlatformUserSettingsProfile>(Loaded);
                    if (!Profile || Profile->SchemaVersion < 1)
                    {
                        Completion({}, FGamePlatformResult::Failure(TEXT("SettingsProfileCorrupt"), TEXT("本地用户档案损坏或类型不兼容；保留旧只读快照且不覆盖原文件。")));
                        return;
                    }
                    FGamePlatformSettingsPersistencePayload Payload;
                    Payload.SchemaVersion = Profile->SchemaVersion;
                    Payload.Layers.Add(EGamePlatformSettingLayer::User, Profile->UserValues);
                    Completion(MoveTemp(Payload), FGamePlatformResult::Success());
                }));
        });
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
