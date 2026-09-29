#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformServerAdmissionContractTest,
    "GamePlatform.Server.Admission.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformServerAdmissionContractTest::RunTest(const FString&)
{
    FGamePlatformServerAdmissionTarget Target;
    Target.GameServerId = TEXT("server-001");
    Target.ServerBootId = TEXT("boot-001");
    Target.WorldId = TEXT("World.OpenWorld.Main");
    Target.ExperienceId = TEXT("Experience.OpenWorld.Main");
    Target.ProtocolVersion = TEXT("2");
    Target.ServerStartGeneration = 1;
    TestTrue(TEXT("完整服务器准入目标有效"), Target.IsValid());

    FGamePlatformServerVerifiedAdmission Admission;
    Admission.AdmissionId = FGuid::NewGuid();
    Admission.ConnectionId = FGuid::NewGuid();
    Admission.PlayerId = TEXT("player-001");
    Admission.SessionId = TEXT("session-001");
    Admission.AssignmentId = TEXT("assignment-001");
    Admission.ReservationId = TEXT("reservation-001");
    Admission.ServerInstanceId = Target.GameServerId;
    Admission.ServerBootId = Target.ServerBootId;
    Admission.WorldId = Target.WorldId;
    Admission.ExperienceId = Target.ExperienceId;
    Admission.ProtocolVersion = Target.ProtocolVersion;
    Admission.ConnectionGeneration = 1;
    Admission.SessionEpoch = 3;
    Admission.AuthorityUntil = FDateTime::UtcNow() + FTimespan::FromSeconds(10);
    TestTrue(TEXT("完整权威准入投影有效"), Admission.IsStructurallyValid());
    TestTrue(TEXT("准入必须匹配当前实例Boot/World/Experience/Protocol"), Admission.MatchesTarget(Target));

    Admission.ServerBootId = TEXT("old-boot");
    TestFalse(TEXT("旧Boot准入必须拒绝"), Admission.MatchesTarget(Target));
    Admission.ServerBootId = Target.ServerBootId;
    Admission.SessionEpoch = 0;
    TestFalse(TEXT("零SessionEpoch不是权威准入"), Admission.IsStructurallyValid());

    FGamePlatformServerAdmissionProof Proof;
    Proof.OperationId = FGuid::NewGuid();
    Proof.ReservationId = TEXT("reservation-001");
    Proof.AttemptId = TEXT("attempt-001");
    Proof.Credential.Init(0x5a, 32);
    TestTrue(TEXT("最小32字节准入证明有效"), Proof.IsValid());
    Proof.ResetSensitive();
    TestTrue(TEXT("显式清理后证明字节已释放"), Proof.Credential.IsEmpty());
    return true;
}

#endif
