#include "Commandlets/GamePlatformValidationCommandlet.h"

#include "Audit/GamePlatformAssetAudit.h"
#include "Misc/Parse.h"
#include "Validation/GamePlatformGlobalAssetValidator.h"
#include "Validation/GamePlatformStaticValidators.h"
#include "Validation/GamePlatformValidationService.h"

UGamePlatformValidationCommandlet::UGamePlatformValidationCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UGamePlatformValidationCommandlet::Main(const FString& Params)
{
    FString Mode;
    FString ClientArtifactRoot;
    FString ServerArtifactRoot;
    FString RunId;

    FParse::Value(*Params, TEXT("Mode="), Mode);
    FParse::Value(*Params, TEXT("ClientArtifactRoot="), ClientArtifactRoot);
    FParse::Value(*Params, TEXT("ServerArtifactRoot="), ServerArtifactRoot);
    FParse::Value(*Params, TEXT("RunId="), RunId);

    if (Mode.IsEmpty())
    {
        Mode = TEXT("Architecture");
    }

    if (Mode.Equals(TEXT("Data"), ESearchCase::IgnoreCase))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("资产Data Validation请使用官方 -run=DataValidation；GamePlatformValidation不重复实现平行资产验证框架。"));
        return 2;
    }

    FGamePlatformValidationRunSummary Summary;
    Summary.RunId = RunId.IsEmpty() ? FGamePlatformValidationService::CreateRunId() : RunId;
    Summary.StartedAt = FDateTime::UtcNow();

    const bool bArchitecture =
        Mode.Equals(TEXT("Architecture"), ESearchCase::IgnoreCase)
        || Mode.Equals(TEXT("Release"), ESearchCase::IgnoreCase);

    const bool bCook =
        Mode.Equals(TEXT("Cook"), ESearchCase::IgnoreCase)
        || Mode.Equals(TEXT("Release"), ESearchCase::IgnoreCase);

    const bool bAudit = Mode.Equals(TEXT("Audit"), ESearchCase::IgnoreCase);
    const bool bGlobalAssets = bAudit || Mode.Equals(TEXT("Release"), ESearchCase::IgnoreCase);

    TArray<FGamePlatformAssetAuditRow> AuditRows;

    if (bArchitecture)
    {
        UGamePlatformDependencyValidator* DependencyValidator = NewObject<UGamePlatformDependencyValidator>();
        DependencyValidator->ValidateWorkspace(Summary.Results);

        UGamePlatformInheritanceBoundaryValidator* InheritanceValidator = NewObject<UGamePlatformInheritanceBoundaryValidator>();
        InheritanceValidator->ValidateWorkspace(Summary.Results);

        UGamePlatformRPCAndAuthorityValidator* RpcValidator = NewObject<UGamePlatformRPCAndAuthorityValidator>();
        RpcValidator->ValidateWorkspace(Summary.Results);
    }

    if (bCook)
    {
        UGamePlatformClientLeakValidator* ClientLeakValidator = NewObject<UGamePlatformClientLeakValidator>();
        Summary.Results.Add(ClientLeakValidator->ValidateArtifactRoot(ClientArtifactRoot));

        UGamePlatformServerLeakValidator* ServerLeakValidator = NewObject<UGamePlatformServerLeakValidator>();
        Summary.Results.Add(ServerLeakValidator->ValidateArtifactRoot(ServerArtifactRoot));
    }

    if (bGlobalAssets)
    {
        FGamePlatformGlobalAssetValidator::ValidateProject(Summary.Results);
    }

    if (bAudit)
    {
        FString Error;

        FGamePlatformValidationResult Result;
        Result.RuleId = TEXT("GP.Content");
        Result.Target = TEXT("/Game");

        if (FGamePlatformAssetAudit::AuditFolder(TEXT("/Game"), AuditRows, Error))
        {
            Result.Status = EGamePlatformValidationStatus::Passed;
            Result.Message = FString::Printf(TEXT("Asset Audit读取%d个资产元数据。"), AuditRows.Num());
            Result.Evidence = TEXT("AssetRegistry metadata only; no blanket UObject load.");
        }
        else
        {
            Result.Status = EGamePlatformValidationStatus::Failed;
            Result.Message = Error;
        }

        Summary.Results.Add(MoveTemp(Result));
    }

    if (!bArchitecture && !bCook && !bAudit)
    {
        UE_LOG(LogTemp, Error, TEXT("未知Mode：%s。支持Architecture/Cook/Release/Audit。"), *Mode);
        return 2;
    }

    FGamePlatformValidationService::ApplyAllowlist(Summary.Results);

    Summary.FinishedAt = FDateTime::UtcNow();

    FString ReportDirectory;
    FString ReportError;
    if (!FGamePlatformValidationService::WriteReports(Summary, ReportDirectory, ReportError))
    {
        UE_LOG(LogTemp, Error, TEXT("验证报告写入失败：%s"), *ReportError);
        return 3;
    }

    if (bArchitecture)
    {
        UGamePlatformDependencyValidator* DependencyValidator =
            NewObject<UGamePlatformDependencyValidator>();
        FString GraphError;
        if (!DependencyValidator->WriteDependencyGraphJson(
            ReportDirectory / TEXT("dependency-graph.json"),
            GraphError))
        {
            UE_LOG(LogTemp, Error, TEXT("依赖图证据写入失败：%s"), *GraphError);
            return 4;
        }
    }

    if (bAudit && !AuditRows.IsEmpty())
    {
        FString AuditError;
        if (!FGamePlatformAssetAudit::WriteCsv(
            AuditRows,
            ReportDirectory / TEXT("asset-audit.csv"),
            AuditError))
        {
            UE_LOG(LogTemp, Error, TEXT("资产审计CSV写入失败：%s"), *AuditError);
            return 5;
        }
    }

    const FName GateStage =
        Mode.Equals(TEXT("Release"), ESearchCase::IgnoreCase)
            ? FName(TEXT("Release"))
            : (Mode.Equals(TEXT("Architecture"), ESearchCase::IgnoreCase)
                ? FName(TEXT("PreSubmit"))
                : FName(TEXT("Nightly")));

    int32 BlockingFailures = 0;
    for (const FGamePlatformValidationResult& Result : Summary.Results)
    {
        if (FGamePlatformValidationService::ShouldBlockResult(Result, GateStage))
        {
            ++BlockingFailures;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("GamePlatformValidation完成：Mode=%s RunId=%s BlockingFailures=%d Report=%s"),
        *Mode,
        *Summary.RunId,
        BlockingFailures,
        *ReportDirectory);

    return BlockingFailures == 0 ? 0 : 1;
}
