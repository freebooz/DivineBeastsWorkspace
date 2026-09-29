#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Types/GamePlatformAdmissionHandshake.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformAdmissionHandshakeContractTest,
    "GamePlatform.Gameplay.AdmissionHandshake.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformAdmissionHandshakeContractTest::RunTest(const FString&)
{
    FGamePlatformAdmissionProofEnvelope Proof;
    Proof.OperationId = FGuid::NewGuid();
    Proof.ReservationId = TEXT("ticket-001");
    Proof.AttemptId = TEXT("attempt-001");
    Proof.Credential.Init(0x5a, 32);

    TestTrue(TEXT("完整一次性准入证明结构有效"), Proof.IsStructurallyValid());
    Proof.ResetSensitive();
    TestTrue(TEXT("敏感证明显式清理后字节释放"), Proof.Credential.IsEmpty());
    TestFalse(TEXT("清理后的证明不能再次提交"), Proof.IsStructurallyValid());

    FGamePlatformAdmissionConfirmation Confirmation;
    Confirmation.OperationId = FGuid::NewGuid();
    Confirmation.AssignmentId = TEXT("assignment-001");
    Confirmation.GameSessionId = TEXT("game-session-001");
    Confirmation.ServerInstanceId = TEXT("server-001");
    Confirmation.ServerBootId = TEXT("boot-001");
    Confirmation.WorldId = TEXT("World.OpenWorld.Main");
    Confirmation.ProtocolVersion = TEXT("2");
    Confirmation.SessionEpoch = 5;
    TestTrue(TEXT("完整非敏感准入确认有效"), Confirmation.IsStructurallyValid());

    Confirmation.SessionEpoch = 0;
    TestFalse(TEXT("零SessionEpoch不能形成权威确认"), Confirmation.IsStructurallyValid());
    Confirmation.SessionEpoch = 5;
    Confirmation.ServerBootId.Reset();
    TestFalse(TEXT("缺失ServerBootId必须拒绝"), Confirmation.IsStructurallyValid());

    return true;
}

#endif
