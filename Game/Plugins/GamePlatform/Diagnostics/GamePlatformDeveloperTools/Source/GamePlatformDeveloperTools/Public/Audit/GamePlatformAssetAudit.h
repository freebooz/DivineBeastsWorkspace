#pragma once

#include "CoreMinimal.h"
#include "GamePlatformAssetAudit.generated.h"

/** FGamePlatformAssetAuditRow（资产审计行）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformAssetAuditRow
{
    GENERATED_BODY()

    UPROPERTY() FString PackageName;
    UPROPERTY() FString AssetName;
    UPROPERTY() FString ClassName;
    UPROPERTY() FString PrimaryAssetType;
    UPROPERTY() FString ChunkIds;
    UPROPERTY() int64 DiskSize = 0;
};

/** FGamePlatformAssetAudit（资产审计工具）；使用Asset Registry元数据，默认不加载全部UObject。 */
class GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformAssetAudit
{
public:
    static bool AuditFolder(FName PackagePath, TArray<FGamePlatformAssetAuditRow>& OutRows, FString& OutError);
    static bool AuditDependencies(FName PackageName, TArray<FName>& OutDependencies, FString& OutError);
    static bool WriteCsv(const TArray<FGamePlatformAssetAuditRow>& Rows, const FString& OutputPath, FString& OutError);
};
