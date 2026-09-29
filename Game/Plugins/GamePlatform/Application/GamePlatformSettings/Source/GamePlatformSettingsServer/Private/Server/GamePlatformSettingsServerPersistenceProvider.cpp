#include "Server/GamePlatformSettingsServerPersistenceProvider.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Settings/GamePlatformSettingsProjectSettings.h"

namespace
{
    constexpr TCHAR ServerDefaultSection[] =
        TEXT("GamePlatformSettings.ServerDefault");
    constexpr TCHAR DeploymentSection[] =
        TEXT("GamePlatformSettings.Deployment");
}

FName FGamePlatformSettingsServerPersistenceProvider::GetPersistenceId() const
{
    return FName(TEXT("GamePlatformSettingsServerConfig"));
}

bool FGamePlatformSettingsServerPersistenceProvider::SupportsRuntime(
    const EGamePlatformSettingRuntimeScope RuntimeScope) const
{
    return RuntimeScope == EGamePlatformSettingRuntimeScope::Server;
}

FString FGamePlatformSettingsServerPersistenceProvider::MakeEnvironmentKey(
    const FName SettingId)
{
    FString Result = TEXT("GP_SETTING_");
    FString Body = SettingId.ToString().ToUpper();

    for (TCHAR& Character : Body)
    {
        if (!FChar::IsAlnum(Character))
        {
            Character = TEXT('_');
        }
    }

    Result += Body;
    return Result;
}

FGamePlatformResult
FGamePlatformSettingsServerPersistenceProvider::ParseAndAdd(
    const FGamePlatformSettingDescriptor& Descriptor,
    const FString& RawValue,
    const EGamePlatformSettingLayer Layer,
    FGamePlatformSettingsPersistencePayload& OutPayload)
{
    FGamePlatformSettingValue Parsed;
    if (!FGamePlatformSettingValue::TryParse(
            Descriptor.ValueType,
            RawValue,
            Parsed))
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsServerOverrideParseFailed"),
            FString::Printf(
                TEXT("服务器设置 %s 的覆盖值无法按声明类型解析；敏感值未输出。"),
                *Descriptor.SettingId.ToString()));
    }

    OutPayload.Layers.FindOrAdd(Layer)
        .Add(Descriptor.SettingId, MoveTemp(Parsed));
    return FGamePlatformResult::Success();
}

FGamePlatformResult FGamePlatformSettingsServerPersistenceProvider::Load(
    const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
    FGamePlatformSettingsPersistencePayload& OutPayload)
{
    check(IsInGameThread());

    const UGamePlatformSettingsProjectSettings* ProjectSettings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    OutPayload.SchemaVersion =
        ProjectSettings ? ProjectSettings->SchemaVersion : 1;

    if (!IsRunningDedicatedServer())
    {
        return FGamePlatformResult::Success();
    }

    for (const TPair<FName, FGamePlatformSettingDescriptor>& Pair :
        Descriptors)
    {
        const FGamePlatformSettingDescriptor& Descriptor = Pair.Value;
        if (Descriptor.RuntimeScope != EGamePlatformSettingRuntimeScope::Server &&
            Descriptor.PersistenceScope != EGamePlatformSettingScope::Server)
        {
            continue;
        }

        if (Descriptor.bSensitive)
        {
            return FGamePlatformResult::Failure(
                TEXT("SettingsSensitiveServerOverrideUnsupported"),
                TEXT("敏感设置禁止通过INI、环境变量或命令行注入；凭据和密钥必须使用部署秘密机制。"));
        }

        FString Raw;
        if (GConfig &&
            GConfig->GetString(
                ServerDefaultSection,
                *Descriptor.SettingId.ToString(),
                Raw,
                GGameIni))
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    Raw,
                    EGamePlatformSettingLayer::ServerDefault,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }

        Raw.Reset();
        if (GConfig &&
            GConfig->GetString(
                DeploymentSection,
                *Descriptor.SettingId.ToString(),
                Raw,
                GGameIni))
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    Raw,
                    EGamePlatformSettingLayer::Deployment,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }

        Raw = FPlatformMisc::GetEnvironmentVariable(
            *MakeEnvironmentKey(Descriptor.SettingId));
        if (!Raw.IsEmpty())
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    Raw,
                    EGamePlatformSettingLayer::Environment,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }

        Raw.Reset();
        const FString CommandPrefix =
            FString::Printf(
                TEXT("GPSetting.%s="),
                *Descriptor.SettingId.ToString());
        if (FParse::Value(
                FCommandLine::Get(),
                *CommandPrefix,
                Raw))
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    Raw,
                    EGamePlatformSettingLayer::CommandLine,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult
FGamePlatformSettingsServerPersistenceProvider::BeginSave(
    const TMap<FName, FGamePlatformSettingValue>& UserValues,
    const int32 SchemaVersion,
    FGamePlatformSettingsSaveCompletion Completion)
{
    return FGamePlatformResult::Unsupported(
        TEXT("SettingsServerPersistenceReadOnly"),
        TEXT("Dedicated Server设置来自部署配置，运行时不回写服务器配置文件。"));
}
