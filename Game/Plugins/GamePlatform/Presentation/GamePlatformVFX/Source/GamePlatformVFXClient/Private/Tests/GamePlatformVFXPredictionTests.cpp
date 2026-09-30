// F17：自然完成后确认不重播，先取消后预测不复活；容量/期限属于世界局部历史。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Engine/World.h"
#include "Settings/GamePlatformVFXSettings.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXPredictionTerminalTest, "GamePlatform.VFX.Prediction.TerminalHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXPredictionTerminalTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    auto* Service=NewObject<UGamePlatformVFXWorldSubsystem>(World.Get());
    FGamePlatformVFXRequest Request; Request.RequestId=FGuid::NewGuid(); Request.DefinitionId=TEXT("presentation.vfx.hit@1");
    const auto Key=Service->MakeDedupeKey(Request);
    const auto Handle=Service->InstanceRegistry.Reserve(World.Get());
    Service->AddDedupeHandle(Key, Handle); Service->CleanupInstance(Handle, false);
    Request.PredictionState=EGamePlatformVFXPredictionState::Confirmed;
    TestEqual(TEXT("自然结束后确认只确认历史"), Service->Play(Request).Code, EGamePlatformVFXResultCode::AlreadyCompleted);
    Request.RequestId=FGuid::NewGuid(); Request.PredictionState=EGamePlatformVFXPredictionState::Cancelled;
    Service->Play(Request); Request.PredictionState=EGamePlatformVFXPredictionState::Predicted;
    TestEqual(TEXT("先取消后迟到预测仍取消"), Service->Play(Request).Code, EGamePlatformVFXResultCode::Cancelled);
    Request.PredictionState=EGamePlatformVFXPredictionState::Corrected;
    TestEqual(TEXT("最终取消不能被纠正复活"), Service->Play(Request).Code, EGamePlatformVFXResultCode::Cancelled);
    const int32 Limit=FMath::Max(1,GetDefault<UGamePlatformVFXSettings>()->MaxDedupeEntries);
    for (int32 Index=0;Index<Limit+1;++Index) { Request.RequestId=FGuid::NewGuid(); Service->RecordTerminalOccurrence(Service->MakeDedupeKey(Request),false); }
    TestEqual(TEXT("历史容量有界"),Service->TerminalOccurrences.Num(),Limit);
    for (auto& Pair:Service->TerminalOccurrences) Pair.Value.ExpiresAtSeconds=0.0;
    Service->PruneTerminalOccurrences(); TestEqual(TEXT("到期记录撤销"),Service->TerminalOccurrences.Num(),0);
    World->DestroyWorld(false); return true;
}
#endif
