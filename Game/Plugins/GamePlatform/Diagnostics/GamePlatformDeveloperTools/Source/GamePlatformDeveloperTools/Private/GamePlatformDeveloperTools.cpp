#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "DataValidationModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "IContentBrowserSingleton.h"
#include "Misc/App.h"
#include "Misc/DataValidation.h"
#include "Misc/EngineVersion.h"
#include "Performance/GamePlatformPerformanceTestRunner.h"
#include "Review/GamePlatformReviewTypes.h"
#include "ToolMenus.h"
#include "Validation/GamePlatformStaticValidators.h"
#include "Validation/GamePlatformGlobalAssetValidator.h"
#include "Validation/GamePlatformValidationService.h"

#define LOCTEXT_NAMESPACE "GamePlatformDeveloperTools"

class FGamePlatformDeveloperToolsModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGamePlatformValidationService::RegisterBuiltInRules();

        if (!IsRunningCommandlet())
        {
            UToolMenus::RegisterStartupCallback(
                FSimpleMulticastDelegate::FDelegate::CreateRaw(
                    this,
                    &FGamePlatformDeveloperToolsModule::RegisterMenus));
        }
    }

    virtual void ShutdownModule() override
    {
        if (!IsRunningCommandlet())
        {
            UToolMenus::UnRegisterStartupCallback(this);
            UToolMenus::UnregisterOwner(this);
        }

        FGamePlatformValidationService::ResetForShutdown();
    }

private:
    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);

        UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
        if (!ToolsMenu)
        {
            return;
        }

        UToolMenu* ValidationMenu = ToolsMenu->AddSubMenu(
            this,
            "GamePlatformValidation",
            "GamePlatformValidation",
            LOCTEXT("MenuLabel", "Game Platform Validation（游戏平台验证）"),
            LOCTEXT("MenuTooltip", "资产验证、架构门禁、端侧泄漏、人工验收与性能证据入口。"));
        PopulateValidationMenu(ValidationMenu);
    }

    void PopulateValidationMenu(UToolMenu* Menu)
    {
        if (!Menu)
        {
            return;
        }

        FToolMenuSection& Section = Menu->AddSection(
            "GamePlatformValidationActions",
            LOCTEXT("SectionLabel", "Validation Actions（验证操作）"));

        const struct FMenuSpec
        {
            const TCHAR* Name;
            const TCHAR* Label;
            const TCHAR* Description;
        } Specs[] =
        {
            { TEXT("ValidateSelected"), TEXT("Validate Selected（验证所选资产）"), TEXT("复用UE Data Validation验证当前选择。") },
            { TEXT("ValidateFolder"), TEXT("Validate Folder（验证目录）"), TEXT("目录级验证与资产审计。") },
            { TEXT("ValidateProject"), TEXT("Validate Project（验证项目）"), TEXT("运行项目级数据验证与质量门禁。") },
            { TEXT("ValidateArchitecture"), TEXT("Validate Architecture（验证架构）"), TEXT("检查三层依赖、循环依赖与端侧边界。") },
            { TEXT("AuditServerAssets"), TEXT("Audit Server Assets（审计服务器资产）"), TEXT("检查Server-safe引用与客户端表现泄漏。") },
            { TEXT("AuditClientAssets"), TEXT("Audit Client Assets（审计客户端资产）"), TEXT("检查客户端Cook/构建工件中的ServerOnly内容。") },
            { TEXT("RunReviewCases"), TEXT("Run Review Cases（运行人工验收用例）"), TEXT("只创建/整理待人工记录，不自动代签Passed。") },
            { TEXT("PerformanceTests"), TEXT("Performance Tests（性能测试）"), TEXT("运行预定义场景并与兼容基线比较。") },
            { TEXT("OpenLatestReport"), TEXT("Open Latest Report（查看最新报告）"), TEXT("报告目录为Saved/GamePlatformValidation。") }
        };

        for (const FMenuSpec& Spec : Specs)
        {
            Section.AddMenuEntry(
                FName(Spec.Name),
                FText::FromString(Spec.Label),
                FText::FromString(Spec.Description),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateRaw(
                    this,
                    &FGamePlatformDeveloperToolsModule::ExecuteMenuAction,
                    FString(Spec.Name))));
        }
    }

    static bool IsProjectOwnedAsset(const FAssetData& Asset)
    {
        const FString Package = Asset.PackageName.ToString();
        return Package.StartsWith(TEXT("/Game/"))
            || Package.StartsWith(TEXT("/GamePlatform"))
            || Package.StartsWith(TEXT("/DBA"));
    }

    static void RunDataValidation(const TArray<FAssetData>& Assets, bool bDependencies)
    {
        if (Assets.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("GamePlatformDeveloperTools：没有可验证资产。"));
            return;
        }

        IDataValidationModule::Get().ValidateAssets(
            Assets,
            bDependencies,
            EDataValidationUsecase::Manual);
    }

    TArray<FAssetData> GetSelectedAssets() const
    {
        TArray<FAssetData> Assets;
        FContentBrowserModule& ContentBrowser =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        ContentBrowser.Get().GetAllSelectedAssets(Assets);
        return Assets;
    }

    TArray<FAssetData> GetSelectedFolderAssets() const
    {
        TArray<FString> Folders;
        FContentBrowserModule& ContentBrowser =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        ContentBrowser.Get().GetSelectedFolders(Folders);

        FAssetRegistryModule& RegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& Registry = RegistryModule.Get();

        TArray<FAssetData> Assets;
        for (const FString& Folder : Folders)
        {
            TArray<FAssetData> FolderAssets;
            Registry.GetAssetsByPath(FName(*Folder), FolderAssets, true, true);
            Assets.Append(MoveTemp(FolderAssets));
        }

        return Assets;
    }

    TArray<FAssetData> GetProjectAssets() const
    {
        FAssetRegistryModule& RegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& Registry = RegistryModule.Get();

        TArray<FAssetData> AllAssets;
        Registry.GetAllAssets(AllAssets, true);

        TArray<FAssetData> ProjectAssets;
        ProjectAssets.Reserve(AllAssets.Num());
        for (const FAssetData& Asset : AllAssets)
        {
            if (IsProjectOwnedAsset(Asset))
            {
                ProjectAssets.Add(Asset);
            }
        }
        return ProjectAssets;
    }

    void RunArchitectureValidation()
    {
        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        UGamePlatformDependencyValidator* DependencyValidator =
            NewObject<UGamePlatformDependencyValidator>(GetTransientPackage());
        DependencyValidator->ValidateWorkspace(Summary.Results);

        UGamePlatformRPCAndAuthorityValidator* RpcValidator =
            NewObject<UGamePlatformRPCAndAuthorityValidator>(GetTransientPackage());
        RpcValidator->ValidateWorkspace(Summary.Results);

        FGamePlatformValidationService::ApplyAllowlist(Summary.Results);

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString Error;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            Error))
        {
            UE_LOG(LogTemp, Error, TEXT("架构验证报告写入失败：%s"), *Error);
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("架构验证完成：RunId=%s Report=%s"),
            *Summary.RunId,
            *ReportDirectory);
    }

    void RunGlobalAssetValidation()
    {
        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        FGamePlatformGlobalAssetValidator::ValidateProject(Summary.Results);

        FGamePlatformValidationService::ApplyAllowlist(Summary.Results);

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString Error;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            Error))
        {
            UE_LOG(LogTemp, Error, TEXT("全局资产验证报告写入失败：%s"), *Error);
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("全局资产验证完成：RunId=%s Report=%s"),
            *Summary.RunId,
            *ReportDirectory);
    }

    void ValidateServerSafeAssets()
    {
        TArray<FAssetData> ServerAssets;
        for (const FAssetData& Asset : GetProjectAssets())
        {
            const FString Package = Asset.PackageName.ToString();
            if (Package.Contains(TEXT("/Server/"))
                || Package.Contains(TEXT("/ServerSafe/")))
            {
                ServerAssets.Add(Asset);
            }
        }

        RunDataValidation(ServerAssets, true);
    }

    void ValidateClientDomainAssets()
    {
        TArray<FAssetData> ClientAssets;
        for (const FAssetData& Asset : GetProjectAssets())
        {
            const FString Package = Asset.PackageName.ToString();
            if (Package.Contains(TEXT("/Client/"))
                || Package.Contains(TEXT("/Presentation/"))
                || Package.Contains(TEXT("/UI/"))
                || Package.Contains(TEXT("/VFX/"))
                || Package.Contains(TEXT("/SFX/")))
            {
                ClientAssets.Add(Asset);
            }
        }

        RunDataValidation(ClientAssets, true);
    }

    void RunSelectedReviewCases()
    {
        const TArray<FAssetData> SelectedAssets = GetSelectedAssets();

        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        UGamePlatformReviewSubsystem* ReviewSubsystem =
            GEditor
                ? GEditor->GetEditorSubsystem<UGamePlatformReviewSubsystem>()
                : nullptr;

        int32 CaseCount = 0;
        for (const FAssetData& AssetData : SelectedAssets)
        {
            UGamePlatformReviewCase* ReviewCase =
                Cast<UGamePlatformReviewCase>(AssetData.GetAsset());
            if (!ReviewCase)
            {
                continue;
            }

            ++CaseCount;
            FGamePlatformValidationResult Result;
            Result.RuleId = TEXT("GP.ManualReview");
            Result.Target = ReviewCase->CaseId.IsNone()
                ? AssetData.PackageName.ToString()
                : ReviewCase->CaseId.ToString();

            FString CaseError;
            if (!ReviewCase->IsCaseComplete(CaseError))
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("Review Case（人工验收用例）不完整：") + CaseError;
                Result.SuggestedFix = TEXT("补齐CaseId/Category/Description/Steps/ExpectedResult/RequiredEvidence。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            if (!ReviewSubsystem)
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("UGamePlatformReviewSubsystem（人工验收子系统）不可用。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            UGamePlatformReviewReport* Pending =
                ReviewSubsystem->CreatePendingReport(
                    ReviewCase,
                    Summary.RunId,
                    FApp::GetBuildVersion(),
                    TEXT("non-git-unavailable"));

            if (!Pending)
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("无法创建待人工Review Report（验收报告）。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            Result.Status = EGamePlatformValidationStatus::NotRun;
            Result.Message = TEXT("Review Report已创建为NotRun（待人工）；真实Reviewer完成前不得改为Passed。");
            Result.Evidence = FString::Join(ReviewCase->RequiredEvidence, TEXT("; "));
            Summary.Results.Add(MoveTemp(Result));
        }

        if (CaseCount == 0)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Run Review Cases：请先在Content Browser（内容浏览器）选择UGamePlatformReviewCase资产。"));
            return;
        }

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString Error;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            Error))
        {
            UE_LOG(LogTemp, Error, TEXT("人工验收待办报告写入失败：%s"), *Error);
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("已生成%d个人工验收待办，状态保持NotRun：%s"),
            CaseCount,
            *ReportDirectory);
    }

    void RunSelectedPerformanceProfiles()
    {
        const TArray<FAssetData> SelectedAssets = GetSelectedAssets();

        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        TArray<FGamePlatformPerformanceRunResult> CompletedRuns;
        int32 ProfileCount = 0;

        UGamePlatformPerformanceTestRunner* Runner =
            NewObject<UGamePlatformPerformanceTestRunner>(GetTransientPackage());

        for (const FAssetData& AssetData : SelectedAssets)
        {
            UGamePlatformPerformanceProfileAsset* ProfileAsset =
                Cast<UGamePlatformPerformanceProfileAsset>(AssetData.GetAsset());
            if (!ProfileAsset)
            {
                continue;
            }

            ++ProfileCount;
            const FGamePlatformPerformanceProfile& Profile = ProfileAsset->Profile;

            FGamePlatformValidationResult Result;
            Result.RuleId = TEXT("GP.Performance");
            Result.Target = Profile.ScenarioId.IsNone()
                ? AssetData.PackageName.ToString()
                : Profile.ScenarioId.ToString();

            FString Error;
            if (!Runner->IsProfileValid(Profile, Error))
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("Performance Profile（性能场景配置）无效：") + Error;
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            if (!FGamePlatformPerformanceExecutorRegistry::IsRegistered())
            {
                Result.Status = EGamePlatformValidationStatus::NotRun;
                Result.Message =
                    TEXT("Performance Profile有效，但当前未注册项目层Scenario Executor（场景执行器），因此未执行真实性能场景。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            FGamePlatformPerformanceRunResult RunResult;
            const FString HardwareProfile = FString::Printf(
                TEXT("%s | %s"),
                *FPlatformMisc::GetCPUBrand(),
                *FPlatformMisc::GetPrimaryGPUBrand());

            const bool bRan = Runner->RunProfile(
                Profile,
                FApp::GetBuildVersion(),
                TEXT("non-git-unavailable"),
                FEngineVersion::Current().ToString(),
                HardwareProfile,
                [](const FGamePlatformPerformanceProfile& InProfile,
                   TMap<FName, TArray<double>>& OutSamples,
                   FString& OutError)
                {
                    return FGamePlatformPerformanceExecutorRegistry::Execute(
                        InProfile,
                        OutSamples,
                        OutError);
                },
                RunResult,
                Error);

            if (!bRan)
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("性能场景执行失败：") + Error;
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            Result.Status = EGamePlatformValidationStatus::Passed;
            Result.Message = FString::Printf(
                TEXT("性能场景已真实执行并采集%d类指标。"),
                RunResult.Metrics.Num());
            Result.Evidence = TEXT("performance.json");
            Summary.Results.Add(MoveTemp(Result));
            CompletedRuns.Add(MoveTemp(RunResult));
        }

        if (ProfileCount == 0)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Performance Tests：请先选择UGamePlatformPerformanceProfileAsset（性能场景配置资产）。"));
            return;
        }

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString ReportError;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            ReportError))
        {
            UE_LOG(LogTemp, Error, TEXT("性能验证报告写入失败：%s"), *ReportError);
            return;
        }

        for (int32 Index = 0; Index < CompletedRuns.Num(); ++Index)
        {
            const FString FileName =
                Index == 0
                    ? TEXT("performance.json")
                    : FString::Printf(
                        TEXT("performance-%s.json"),
                        *CompletedRuns[Index].ScenarioId.ToString());

            FString Error;
            if (!Runner->WriteResultJson(
                CompletedRuns[Index],
                ReportDirectory / FileName,
                Error))
            {
                UE_LOG(LogTemp, Error, TEXT("性能结果写入失败：%s"), *Error);
            }
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Performance Tests处理%d个Profile；真实执行数=%d；报告=%s"),
            ProfileCount,
            CompletedRuns.Num(),
            *ReportDirectory);
    }

    void OpenLatestReport()
    {
        const FString Root = FGamePlatformValidationService::GetReportRoot();
        TArray<FString> Directories;
        IFileManager::Get().FindFiles(
            Directories,
            *(Root / TEXT("*")),
            false,
            true);

        if (Directories.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("暂无 GamePlatformValidation（游戏平台验证）报告。"));
            return;
        }

        Directories.Sort([](const FString& A, const FString& B)
        {
            return A > B;
        });

        const FString Latest = Root / Directories[0];
        FPlatformProcess::ExploreFolder(*Latest);
    }

    void ExecuteMenuAction(FString ActionName)
    {
        if (ActionName == TEXT("ValidateSelected"))
        {
            RunDataValidation(GetSelectedAssets(), false);
        }
        else if (ActionName == TEXT("ValidateFolder"))
        {
            RunDataValidation(GetSelectedFolderAssets(), false);
        }
        else if (ActionName == TEXT("ValidateProject"))
        {
            RunDataValidation(GetProjectAssets(), false);
            RunGlobalAssetValidation();
        }
        else if (ActionName == TEXT("ValidateArchitecture"))
        {
            RunArchitectureValidation();
        }
        else if (ActionName == TEXT("AuditServerAssets"))
        {
            ValidateServerSafeAssets();
        }
        else if (ActionName == TEXT("AuditClientAssets"))
        {
            ValidateClientDomainAssets();
        }
        else if (ActionName == TEXT("OpenLatestReport"))
        {
            OpenLatestReport();
        }
        else if (ActionName == TEXT("RunReviewCases"))
        {
            RunSelectedReviewCases();
        }
        else if (ActionName == TEXT("PerformanceTests"))
        {
            RunSelectedPerformanceProfiles();
        }
    }
};

IMPLEMENT_MODULE(FGamePlatformDeveloperToolsModule, GamePlatformDeveloperTools)

#undef LOCTEXT_NAMESPACE
