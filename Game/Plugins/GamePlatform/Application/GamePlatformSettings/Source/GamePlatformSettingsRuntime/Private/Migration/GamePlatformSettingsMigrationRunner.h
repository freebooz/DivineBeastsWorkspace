#pragma once

#include "Interfaces/IGamePlatformSettingsProvider.h"

/**
 * 设置版本迁移执行器。
 * 使用工作副本完成完整 Migration 链，只有全部成功才提交，保证失败不会留下半迁移状态。
 */
struct FGamePlatformSettingsMigrationRunner final
{
    static FGamePlatformResult Run(
        TMap<FName, FGamePlatformSettingValue>& InOutUserValues,
        int32& InOutSchemaVersion,
        int32 TargetVersion,
        const TArray<IGamePlatformSettingsMigration*>& Migrations,
        int32& OutAppliedMigrationCount);
};
