#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsContractVersion.generated.h"

/** FDivineBeastsContractCompatibilitySummary（项目契约兼容摘要）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSRUNTIME_API FDivineBeastsContractCompatibilitySummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    FString CurrentVersion;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    int32 CatalogVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    FString GeneratedRevision;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    FString ClientServerMinimumVersion;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    FString ClientServerMaximumExclusiveVersion;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    FString ServerBackendMinimumVersion;

    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts")
    FString ServerBackendMaximumExclusiveVersion;
};

/**
 * FDivineBeastsContractVersion（神兽联盟契约版本查询）。
 * 只提供数据查询和版本区间判断，不执行网络协商或HTTP。
 */
class DIVINEBEASTSRUNTIME_API FDivineBeastsContractVersion
{
public:
    static FString GetCurrentVersion();
    static int32 GetCatalogVersion();
    static FString GetGeneratedRevision();
    static FDivineBeastsContractCompatibilitySummary GetCompatibilitySummary();

    static bool IsClientServerCompatible(const FString& Version);
    static bool IsServerBackendCompatible(const FString& Version);

private:
    static bool IsInRange(
        const FString& Version,
        const FString& MinimumInclusive,
        const FString& MaximumExclusive);
};
