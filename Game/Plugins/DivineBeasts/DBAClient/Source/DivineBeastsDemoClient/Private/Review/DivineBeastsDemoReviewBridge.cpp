// SSLM Editor待审桥：项目知道自己的目录，平台只消费中立用例/实际文件，不反向引用项目。
// 全部游戏线程、只创建内存待审对象；没有审核者/人工Passed参数，不改变原始运行和历史修订。
#include "Review/DivineBeastsDemoReviewBridge.h"
#if WITH_EDITOR
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Review/GamePlatformReviewTypes.h"
#include "Editor.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo
{
bool CreatePendingReviews(UDivineBeastsDemoCatalogSubsystem& Catalog, FName ScenarioId,
    const FGuid& RunId, const FString& IndexPath, TArray<TObjectPtr<UObject>>& OutReports, FString& Error)
{
    check(IsInGameThread()); OutReports.Reset();
    auto* Service = GEditor ? GEditor->GetEditorSubsystem<UGamePlatformReviewSubsystem>() : nullptr;
    FDivineBeastsDemoScenarioDefinition Scenario;
    if (!Service || !Catalog.FindScenarioDefinition(ScenarioId, Scenario) || !RunId.IsValid()) { Error = TEXT("Editor审核服务、独立案例或运行GUID缺失。人工核验保持暂缓。"); return false; }
    TArray<TStrongObjectPtr<UGamePlatformReviewReport>> Pending;
    for (const auto& Review : Catalog.GetReviewCases())
    {
        if (!Scenario.ReviewCaseIds.Contains(Review.ReviewCaseId)) continue;
        TOptional<FDivineBeastsDemoFeatureEntry> Feature;
        for (const auto& Plugin : Catalog.GetPlugins()) for (const auto& Entry : Catalog.GetFeatures(Plugin.PluginId))
            if (Entry.FeatureId == Review.FeatureId) Feature = Entry;
        if (!Feature.IsSet()) { Error = TEXT("待审用例所属功能未完整登记。"); return false; }
        TStrongObjectPtr<UGamePlatformReviewCase> Case(NewObject<UGamePlatformReviewCase>(Service));
        Case->CaseId = Review.ReviewCaseId; Case->CaseVersion = Review.Version; Case->Category = TEXT("PluginDemo");
        Case->FeatureId = Review.FeatureId; Case->ScenarioId = Review.ScenarioId;
        Case->Description = Feature->Purpose; Case->Preconditions = Scenario.Preconditions; Case->Steps = Review.Steps; Case->ExpectedResult = Review.ExpectedResult;
        for (FName Type : Review.RequiredEvidence) Case->RequiredEvidence.Add(Type.ToString());
        TStrongObjectPtr<UGamePlatformReviewReport> Report(Service->CreatePendingReport(Case.Get(), RunId.ToString(EGuidFormats::DigitsWithHyphens), TEXT("unverified"), TEXT("unverified")));
        // 当前尚无资产/构建版本绑定证明；允许展示NotRun，但Passed门禁必须继续拒绝。
        if (!Report.IsValid() || !Service->BindRunEvidence(Report.Get(), IndexPath, Catalog.GetSourceFingerprint(), TEXT("unverified"), Error)) return false;
        Pending.Add(MoveTemp(Report));
    }
    if (Pending.IsEmpty()) { Error = TEXT("案例没有独立待审用例。"); return false; }
    for (const auto& Report : Pending) OutReports.Add(Report.Get());
    return true;
}
}
#endif
