#include "Commandlets/GamePlatformWorldValidationCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Commandlets/WorldValidationResultGate.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "Definitions/GamePlatformWorldDefinition.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformWorldValidation, Log, All);

int32 UGamePlatformWorldValidationCommandlet::Main(const FString& FullCommandLine)
{
    check(IsInGameThread());
    RunId = FGuid::NewGuid();
    SelectedObjectPaths.Reset();
    LastResults = {};
    bProcessedResults = false;
    // 不调用原生Main：本机5.8将流程失败映射为2，本入口明确使用0/1验收合同。
    const bool bPassed = GEditor && ValidateDataImpl(FullCommandLine) && bProcessedResults;
    const auto* Statistics = LastResults.ValidatorStatistics.Find(UGamePlatformWorldDefinitionValidator::StaticClass()->GetClassPathName());
    UE_LOG(LogGamePlatformWorldValidation, Display,
        TEXT("WorldDefinitionValidationResult Version=1 RunId=%s Status=%s Selected=%d Requested=%d Checked=%d Valid=%d Invalid=%d Skipped=%d Unvalidated=%d Warnings=%d External=%d LimitReached=%d WorldValidatorChecked=%d Processed=%d ExitCode=%d"),
        *RunId.ToString(EGuidFormats::DigitsWithHyphens), bPassed ? TEXT("Passed") : TEXT("Failed"),
        SelectedObjectPaths.Num(), LastResults.NumRequested, LastResults.NumChecked, LastResults.NumValid,
        LastResults.NumInvalid, LastResults.NumSkipped, LastResults.NumUnableToValidate, LastResults.NumWarnings,
        LastResults.NumExternalObjects, LastResults.bAssetLimitReached ? 1 : 0,
        Statistics ? Statistics->AssetsValidated : 0, bProcessedResults ? 1 : 0, bPassed ? 0 : 1);
    return bPassed ? 0 : 1;
}

bool UGamePlatformWorldValidationCommandlet::GetAssetsToValidate(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssetDataList)
{
    check(IsInGameThread());
    OutAssetDataList.Reset();
    if (!AssetTypeFilterString.IsEmpty())
    {
        UE_LOG(LogGamePlatformWorldValidation, Display, TEXT("WorldDefinitionValidationRejected Reason=AssetTypeOverrideNotAllowed"));
        return false;
    }
    FARFilter Filter;
    Filter.ClassPaths.Add(UGamePlatformWorldDefinition::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UGamePlatformRegionDefinition::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bIncludeOnlyOnDiskAssets = true;
    // ValidateDataImpl已同步SearchAllAssets；这里不使用全资产列表，不按目录名字猜资产类型。
    AssetRegistry.GetAssets(Filter, OutAssetDataList);
    // 空列表继续交给统一结果门禁，保证输出零执行的机器可读失败汇总。
    return true;
}

void UGamePlatformWorldValidationCommandlet::FilterAssetsToValidate(TArray<FAssetData>& AssetDataList)
{
    Super::FilterAssetsToValidate(AssetDataList);
    SelectedObjectPaths.Reset();
    for (const FAssetData& Asset : AssetDataList) { SelectedObjectPaths.Add(Asset.GetObjectPathString()); }
    SelectedObjectPaths.Sort();
}

void UGamePlatformWorldValidationCommandlet::SetupValidationSettings(FValidateAssetsSettings& Settings)
{
    Super::SetupValidationSettings(Settings);
    Settings.bSkipExcludedDirectories = false;
    Settings.bLoadAssetsForValidation = true;
    Settings.bLoadExternalObjectsForValidation = false;
    Settings.bCollectPerAssetDetails = true;
    Settings.bCaptureAssetLoadLogs = true;
    Settings.bCaptureLogsDuringValidation = true;
    Settings.MaxAssetsToValidate = MAX_int32;
}

bool UGamePlatformWorldValidationCommandlet::ProcessValidationResults(FValidateAssetsResults& Results)
{
    check(IsInGameThread());
    bProcessedResults = true;
    LastResults = Results;
    for (const FString& Path : SelectedObjectPaths)
    {
        const auto* Details = Results.AssetsDetails.Find(Path);
        const TCHAR* Status = !Details ? TEXT("Missing") : Details->Result == EDataValidationResult::Valid
            ? TEXT("Valid") : Details->Result == EDataValidationResult::Invalid ? TEXT("Invalid") : TEXT("NotValidated");
        UE_LOG(LogGamePlatformWorldValidation, Display, TEXT("WorldDefinitionValidationAsset Version=1 RunId=%s Object=\"%s\" Status=%s"),
            *RunId.ToString(EGuidFormats::DigitsWithHyphens), *Path, Status);
    }
    return GamePlatformWorldValidation::PassesResultGate(Results, SelectedObjectPaths);
}
