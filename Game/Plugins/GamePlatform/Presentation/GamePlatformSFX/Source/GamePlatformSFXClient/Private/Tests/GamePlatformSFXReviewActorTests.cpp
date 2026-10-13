// 平台SFX可视审核入口回归：只验证当前World、自有请求与拒绝路径，不假装真实声音已验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Review/GamePlatformSFXReviewActor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSFXReviewActorTest,
    "GamePlatform.SFX.Review.ActorOwnershipAndRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformSFXReviewActorTest::RunTest(const FString& Parameters)
{
    // 没有设备和Data定义时必须拒绝，不得为演示创建隐形音频组件或伪造成功。
    TestFalse(TEXT("无世界不能启动审核"), AGamePlatformSFXReviewActor::IsSupportedReviewWorld(nullptr));
    TStrongObjectPtr<UWorld> Preview(UWorld::CreateWorld(EWorldType::EditorPreview, false));
    TestFalse(TEXT("仅打开地图不应自行发声"), AGamePlatformSFXReviewActor::IsSupportedReviewWorld(Preview.Get()));
    Preview->DestroyWorld(false);

    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    // CreateWorld已初始化PersistentLevel；重复InitializeNewWorld会再次生成同名WorldSettings并崩溃。
    // 没有GameInstance/Data定义，本用例不提交有效音频资源，所以无需二次初始化禁用音频。
    auto* Actor = World->SpawnActor<AGamePlatformSFXReviewActor>();
    if (!TestNotNull(TEXT("能创建真实审核Actor"), Actor))
    {
        World->DestroyWorld(false);
        return false;
    }
    TestFalse(TEXT("音效审核Actor不复制"), Actor->GetIsReplicated());
    TestFalse(TEXT("无逐帧轮询"), Actor->PrimaryActorTick.bCanEverTick);
    TestFalse(TEXT("默认不自动播放"), Actor->bPlayOnBeginPlay);
    TestFalse(TEXT("无定义不能返回播放成功"), Actor->PlayReview());
    TestFalse(TEXT("无定义不得留下请求"), Actor->GetActiveRequestId().IsValid());
    TestFalse(TEXT("无声音实例"), Actor->IsReviewActive());
    Actor->DefinitionId = TEXT("/Game/NotALogicalDefinition");
    TestFalse(TEXT("资产路径不能冒充逻辑身份"), Actor->PlayReview());
    TestFalse(TEXT("失败后没有活动请求"), Actor->GetActiveRequestId().IsValid());
    Actor->StopReview();
    Actor->StopReview();
    TestFalse(TEXT("重复停止不创建新请求"), Actor->GetActiveRequestId().IsValid());
    // Stop应取消尚未开始的自有自动播放，不能等用户停止后又由延迟Timer重新发声。
    World->GetTimerManager().SetTimer(Actor->StartTimer, FTimerDelegate::CreateLambda([]() {}), 5.0f, false);
    TestTrue(TEXT("测试已建立真实延迟自动播放定时器"), World->GetTimerManager().TimerExists(Actor->StartTimer));
    Actor->StopReview();
    TestFalse(TEXT("停止审核必须取消未执行的自动播放"), World->GetTimerManager().TimerExists(Actor->StartTimer));
    World->bIsTearingDown = true;
    TestFalse(TEXT("世界清理时不可重新开始"), Actor->PlayReview());
    World->bIsTearingDown = false;
    Actor->Destroy();
    World->DestroyWorld(false);
    return true;
}
#endif
