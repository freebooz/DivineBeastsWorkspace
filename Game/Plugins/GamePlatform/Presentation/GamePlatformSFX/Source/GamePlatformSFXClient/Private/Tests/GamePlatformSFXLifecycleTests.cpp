// 本文件属于GamePlatform平台层 GamePlatformSFX，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// F11/F17：真实音频停止状态回收活动所有权；失败没有AudioFinished时也须归零。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformSFXWorldSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSFXLifecycleRegressionTest, "GamePlatform.SFX.Lifecycle.StoppedAndTerminal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSFXLifecycleRegressionTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    auto* Service=NewObject<UGamePlatformSFXWorldSubsystem>(World.Get());
    auto* Component=NewObject<UAudioComponent>(World.Get());
    FGamePlatformSFXHandle Handle; Handle.Id=FGuid::NewGuid(); Handle.Generation=1; Handle.World=World.Get();
    FGamePlatformSFXRequest Request; Request.RequestId=FGuid::NewGuid(); Request.DefinitionId=TEXT("presentation.sfx.hit@1");
    UGamePlatformSFXWorldSubsystem::FActiveInstance Active;
    Active.Handle=Handle; Active.RequestId=Request.RequestId; Active.Component=Component;
    Service->ActiveInstances.Add(Handle.Id, Active); Service->RequestHandles.Add(Request.RequestId, Handle);
    Service->HandleAudioPlayStateChanged(Component, EAudioComponentPlayState::Stopped);
    TestEqual(TEXT("启动失败停止态不依赖AudioFinished回收"), Service->GetDiagnostics().ActiveInstances, 0);
    TestEqual(TEXT("终态明确为失败"), Service->GetPlaybackSnapshot(Handle).State, EGamePlatformSFXPlaybackState::Failed);
    Request.PredictionState=EGamePlatformSFXPredictionState::Confirmed;
    TestEqual(TEXT("启动失败后的确认保留失败码且不伪装完成"), Service->Play(Request).Code, EGamePlatformSFXResultCode::SpawnFailed);
    const FGuid Cancelled=FGuid::NewGuid(); Service->StopByRequestId(Cancelled); Request.RequestId=Cancelled;
    Request.PredictionState=EGamePlatformSFXPredictionState::Predicted;
    TestEqual(TEXT("先取消后迟到请求不复活"), Service->Play(Request).Code, EGamePlatformSFXResultCode::Cancelled);
    for (int32 Index=0;Index<513;++Index) Service->RecordTerminalOccurrence(FGuid::NewGuid(),false);
    TestEqual(TEXT("历史最多512项"),Service->TerminalOccurrences.Num(),512);
    for (auto& Pair:Service->TerminalOccurrences) Pair.Value.ExpiresAtSeconds=0.0;
    Service->PruneTerminalOccurrences(); TestEqual(TEXT("到期释放历史"),Service->TerminalOccurrences.Num(),0);
    const TStrongObjectPtr<UGamePlatformSFXWorldSubsystem> ClosingService(NewObject<UGamePlatformSFXWorldSubsystem>(World.Get()));
    FGamePlatformSFXRequest Correction; Correction.RequestId = FGuid::NewGuid(); Correction.DefinitionId = TEXT("presentation.sfx.corrected@1");
    Correction.PredictionState = EGamePlatformSFXPredictionState::Corrected;
    FGamePlatformSFXHandle OldHandle; OldHandle.Id = FGuid::NewGuid(); OldHandle.Generation = 1; OldHandle.World = World.Get();
    UGamePlatformSFXWorldSubsystem::FPendingPlay OldPending; OldPending.Handle = OldHandle; OldPending.Request = Correction;
    ClosingService->PendingPlays.Add(OldHandle.Id, OldPending); ClosingService->RequestHandles.Add(Correction.RequestId, OldHandle);
    ClosingService->AddCompletionHandler(FGamePlatformSFXPlaybackCompleted::FDelegate::CreateLambda(
        [&](const FGamePlatformSFXPlaybackSnapshot&) { ClosingService->Deinitialize(); }));
    TestEqual(TEXT("纠正Stop公开终态关停后拒绝替换"), ClosingService->Play(Correction).Code, EGamePlatformSFXResultCode::InvalidWorld);
    TestEqual(TEXT("关闭后不遗留新Pending"), ClosingService->PendingPlays.Num(), 0);
    TestEqual(TEXT("关闭后不补回请求映射"), ClosingService->RequestHandles.Num(), 0);
    World->DestroyWorld(false); return true;
}
#endif
