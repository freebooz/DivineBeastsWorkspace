// 本文件属于DivineBeasts项目层 DivineBeastsPresentationClient，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// F14项目事务回归：仅Transient账本夹具，不模拟资产成功或生成资源；真实Data/Cook另行验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPresentationActivationRegressionTest,
    "DivineBeasts.Presentation.Activation.FailureCancellationAndOldGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPresentationActivationRegressionTest::RunTest(const FString&)
{
    // LocalPlayer与Viewport的ClassWithin为Engine；仅用真实GEngine作Outer，缺引擎明确失败。
    if (!TestNotNull(TEXT("测试宿主Engine必须存在"), GEngine)) { return false; }
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Service(NewObject<UDivineBeastsPresentationClientSubsystem>(Player.Get()));
    const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Platform(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
    Service->PlatformPresentation = Platform.Get(); Service->ScopeId = FGuid::NewGuid();
    FDivineBeastsPresentationContentPackHandle Handle; Handle.Id = FGuid::NewGuid(); Handle.ScopeId = Service->ScopeId; Handle.Generation = 1;
    UDivineBeastsPresentationClientSubsystem::FPendingPack Pending; Pending.Handle = Handle;
    Pending.Fragment.ContentPackId = TEXT("TestPack"); Service->PendingPacks.Add(Handle.Id, Pending);
    UDivineBeastsPresentationClientSubsystem::FLogicalPreload Preload; Preload.RequestId = Handle.Id;
    Preload.Generation = 7; Preload.PackHandle = Handle; Preload.RemainingLoads = 1;
    FGamePlatformDataLease Lease; Lease.LeaseId = FGuid::NewGuid(); Lease.Generation = 3;
    Preload.Leases.Add(Lease); Service->LogicalPreloads.Add(Handle.Id, Preload);
    FString Error;
    TestEqual(TEXT("预载阶段不计Active"), Service->GetActiveContentPackCount(), 0);
    TestEqual(TEXT("预载阶段为Loading"), Service->GetContentPackState(Handle, Error), EDivineBeastsPresentationContentPackState::Loading);
    TestEqual(TEXT("预载阶段目录不可见"), Platform->GetCatalogFragmentCount(), 0);
    Service->HandleLogicalPreloadCompleted(Handle.Id, 6, Lease, FGamePlatformResult::Success());
    TestTrue(TEXT("旧代回执不消耗当前等待"), Service->LogicalPreloads.Contains(Handle.Id));
    Service->HandleLogicalPreloadCompleted(Handle.Id, 7, Lease, FGamePlatformResult::Failure(TEXT("TestMissing"), TEXT("仅测试实际失败回滚路径。")));
    TestEqual(TEXT("加载失败形成失败终态"), Service->GetContentPackState(Handle, Error), EDivineBeastsPresentationContentPackState::Failed);
    TestFalse(TEXT("失败撤销全部本次预载账本"), Service->LogicalPreloads.Contains(Handle.Id));
    TestFalse(TEXT("失败不残留Pending包"), Service->PendingPacks.Contains(Handle.Id));
    TestEqual(TEXT("失败没有发布目录"), Platform->GetCatalogFragmentCount(), 0);
    TestFalse(TEXT("失败原因明确"), Error.IsEmpty());
    Handle.Id = FGuid::NewGuid(); Pending.Handle = Handle; Preload.RequestId = Handle.Id; Preload.PackHandle = Handle;
    Service->PendingPacks.Add(Handle.Id, Pending); Service->LogicalPreloads.Add(Handle.Id, Preload);
    TestTrue(TEXT("加载中包可取消"), Service->DeactivateContentPack(Handle));
    Service->HandleLogicalPreloadCompleted(Handle.Id, 7, Lease, FGamePlatformResult::Success());
    TestEqual(TEXT("取消后的迟到成功不得复活"), Service->GetContentPackState(Handle, Error), EDivineBeastsPresentationContentPackState::Cancelled);
    auto WrongScope = Handle; WrongScope.ScopeId = FGuid::NewGuid();
    TestEqual(TEXT("跨LocalPlayer句柄拒绝"), Service->GetContentPackState(WrongScope, Error), EDivineBeastsPresentationContentPackState::Invalid);
    Service->PlatformPresentation = nullptr;
    FDivineBeastsWorldInteractionPresentationFact Fact; Fact.FactId = FGuid::NewGuid(); Fact.OptionId = TEXT("TestInteraction");
    TestEqual(TEXT("缺平台提交明确失败"), Service->SubmitWorldInteractionFact(Fact), EGamePlatformPresentationSubmitResult::ProviderMissing);
    TestEqual(TEXT("同事实重送不能把首次失败伪装Submitted"), Service->SubmitWorldInteractionFact(Fact), EGamePlatformPresentationSubmitResult::ProviderMissing);
    FDivineBeastsVillageFeedbackPresentationFact VillageFact;
    VillageFact.FactId = FGuid::NewGuid(); VillageFact.FeedbackId = TEXT("TestGuidance");
    VillageFact.ExperienceId = TEXT("Experience.Village.Tutorial");
    TestEqual(TEXT("合法教学反馈缺平台仍失败"), Service->SubmitVillageFeedbackFact(VillageFact), EGamePlatformPresentationSubmitResult::ProviderMissing);
    TestEqual(TEXT("教学反馈重复未受理事实仍失败"), Service->SubmitVillageFeedbackFact(VillageFact), EGamePlatformPresentationSubmitResult::ProviderMissing);
    return true;
}
#endif
