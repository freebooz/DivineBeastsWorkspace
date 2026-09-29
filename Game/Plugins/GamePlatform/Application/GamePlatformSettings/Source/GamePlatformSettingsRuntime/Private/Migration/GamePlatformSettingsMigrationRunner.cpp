#include "Migration/GamePlatformSettingsMigrationRunner.h"

FGamePlatformResult FGamePlatformSettingsMigrationRunner::Run(
    TMap<FName, FGamePlatformSettingValue>& InOutUserValues,
    int32& InOutSchemaVersion,
    const int32 TargetVersion,
    const TArray<IGamePlatformSettingsMigration*>& Migrations,
    int32& OutAppliedMigrationCount)
{
    OutAppliedMigrationCount = 0;

    if (InOutSchemaVersion < 1 || TargetVersion < 1)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsMigrationVersionInvalid"),
            TEXT("配置Schema版本必须大于等于1。"));
    }

    if (InOutSchemaVersion > TargetVersion)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsProfileNewerThanRuntime"),
            TEXT("用户设置档案版本高于当前运行时，拒绝降级覆盖。"));
    }

    TMap<FName, FGamePlatformSettingValue> WorkingValues = InOutUserValues;
    int32 WorkingVersion = InOutSchemaVersion;
    int32 Applied = 0;

    while (WorkingVersion < TargetVersion)
    {
        IGamePlatformSettingsMigration* Selected = nullptr;
        for (IGamePlatformSettingsMigration* Migration : Migrations)
        {
            if (!Migration ||
                Migration->GetFromVersion() != WorkingVersion ||
                Migration->GetToVersion() != WorkingVersion + 1)
            {
                continue;
            }

            if (Selected)
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsMigrationAmbiguous"),
                    TEXT("同一版本步存在多个Migration，拒绝按加载顺序选择。"));
            }

            Selected = Migration;
        }

        if (!Selected)
        {
            return FGamePlatformResult::Failure(
                TEXT("SettingsMigrationMissing"),
                TEXT("配置升级链缺少连续的逐版本Migration。"));
        }

        const FGamePlatformResult Result =
            Selected->Migrate(WorkingValues);
        if (!Result.IsSuccess())
        {
            return Result;
        }

        ++WorkingVersion;
        ++Applied;
    }

    InOutUserValues = MoveTemp(WorkingValues);
    InOutSchemaVersion = WorkingVersion;
    OutAppliedMigrationCount = Applied;
    return FGamePlatformResult::Success();
}
