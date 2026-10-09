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
    Request.PredictionState=EGamePlatformSFXPredictionState::Confirmed;
    TestEqual(TEXT("完成后确认不重播"), Service->Play(Request).Code, EGamePlatformSFXResultCode::AlreadyCompleted);
    const FGuid Cancelled=FGuid::NewGuid(); Service->StopByRequestId(Cancelled); Request.RequestId=Cancelled;
    Request.PredictionState=EGamePlatformSFXPredictionState::Predicted;
    TestEqual(TEXT("先取消后迟到请求不复活"), Service->Play(Request).Code, EGamePlatformSFXResultCode::Cancelled);
    for (int32 Index=0;Index<513;++Index) Service->RecordTerminalOccurrence(FGuid::NewGuid(),false);
    TestEqual(TEXT("历史最多512项"),Service->TerminalOccurrences.Num(),512);
    for (auto& Pair:Service->TerminalOccurrences) Pair.Value.ExpiresAtSeconds=0.0;
    Service->PruneTerminalOccurrences(); TestEqual(TEXT("到期释放历史"),Service->TerminalOccurrences.Num(),0);
    World->DestroyWorld(false); return true;
}
#endif
