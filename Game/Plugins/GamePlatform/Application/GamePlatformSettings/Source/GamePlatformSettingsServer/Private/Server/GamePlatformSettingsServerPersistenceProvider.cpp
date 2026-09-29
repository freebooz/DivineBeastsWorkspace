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

FGamePlatformResult FGamePlatformSettingsServerPersistenceProvider::SetUserContext(
    const FString& UserContextKey)
{
    return FGamePlatformResult::Unsupported(
        TEXT("SettingsUserContextServerUnsupported"),
        TEXT("Dedicated Server不使用客户端User Profile上下文。"));
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

    // 环境变量会把SettingId中的非字母数字字符统一转换为下划线；先检测规范化碰撞，禁止两个设置争用同一部署键。
    TMap<FString, FName> EnvironmentOwnerByKey;

    for (const TPair<FName, FGamePlatformSettingDescriptor>& Pair :
        Descriptors)
    {
        const FGamePlatformSettingDescriptor& Descriptor = Pair.Value;
        if (Descriptor.RuntimeScope != EGamePlatformSettingRuntimeScope::Server &&
            Descriptor.PersistenceScope != EGamePlatformSettingScope::Server)
        {
            continue;
        }

        const FString EnvironmentKey =
            MakeEnvironmentKey(Descriptor.SettingId);
        if (const FName* ExistingOwner =
                EnvironmentOwnerByKey.Find(EnvironmentKey);
            ExistingOwner && *ExistingOwner != Descriptor.SettingId)
        {
            return FGamePlatformResult::Failure(
                TEXT("SettingsEnvironmentKeyCollision"),
                TEXT("多个Server SettingId规范化后映射到同一环境变量键，拒绝按遍历顺序决定归属。"));
        }
        EnvironmentOwnerByKey.Add(EnvironmentKey, Descriptor.SettingId);

        FString ServerDefaultRaw;
        const bool bHasServerDefault =
            GConfig &&
            GConfig->GetString(
                ServerDefaultSection,
                *Descriptor.SettingId.ToString(),
                ServerDefaultRaw,
                GGameIni);

        FString DeploymentRaw;
        const bool bHasDeployment =
            GConfig &&
            GConfig->GetString(
                DeploymentSection,
                *Descriptor.SettingId.ToString(),
                DeploymentRaw,
                GGameIni);

        const FString EnvironmentRaw =
            FPlatformMisc::GetEnvironmentVariable(*EnvironmentKey);
        const bool bHasEnvironment = !EnvironmentRaw.IsEmpty();

        FString CommandLineRaw;
        const FString CommandPrefix =
            FString::Printf(
                TEXT("GPSetting.%s="),
                *Descriptor.SettingId.ToString());
        const bool bHasCommandLine =
            FParse::Value(
                FCommandLine::Get(),
                *CommandPrefix,
                CommandLineRaw);

        // 敏感Descriptor可以作为Session临时设置存在；只有真实外部覆盖尝试才拒绝，不能仅因Descriptor存在就阻断服务器启动。
        if (Descriptor.bSensitive)
        {
            if (bHasServerDefault || bHasDeployment ||
                bHasEnvironment || bHasCommandLine)
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsSensitiveServerOverrideUnsupported"),
                    TEXT("敏感设置禁止通过INI、环境变量或命令行注入；凭据和密钥必须使用部署秘密机制。"));
            }
            continue;
        }

        // 按固定优先级顺序逐层解析，避免临时容器分配，也让服务器配置来源更易审计。
        if (bHasServerDefault)
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    ServerDefaultRaw,
                    EGamePlatformSettingLayer::ServerDefault,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }

        if (bHasDeployment)
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    DeploymentRaw,
                    EGamePlatformSettingLayer::Deployment,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }

        if (bHasEnvironment)
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    EnvironmentRaw,
                    EGamePlatformSettingLayer::Environment,
                    OutPayload);
            if (!Result.IsSuccess())
            {
                return Result;
            }
        }

        if (bHasCommandLine)
        {
            const FGamePlatformResult Result =
                ParseAndAdd(
                    Descriptor,
                    CommandLineRaw,
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
