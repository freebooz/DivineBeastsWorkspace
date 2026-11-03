// 原生Mesh生命周期回归：不依赖项目、网络或Niagara资源。
#include "Components/GamePlatformMeshDissolveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformMeshDissolveLifecycleTest,
    "GamePlatform.VFX.MeshDissolve.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformMeshDissolveLifecycleTest::RunTest(const FString&)
{
    const auto Initialization=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,
        ERHIFeatureLevel::Num,&Initialization);
    if(!TestNotNull(TEXT("真实世界"),World))return false;
    AActor* Actor=World->SpawnActor<AActor>();
    auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
    Actor->AddInstanceComponent(Mesh);Mesh->RegisterComponent();
    auto* HiddenMesh=NewObject<UStaticMeshComponent>(Actor);
    Actor->AddInstanceComponent(HiddenMesh);HiddenMesh->RegisterComponent();HiddenMesh->SetVisibility(false);
    auto* Dissolve=NewObject<UGamePlatformMeshDissolveComponent>(Actor);
    Actor->AddInstanceComponent(Dissolve);Dissolve->RegisterComponent();
    FGamePlatformMeshDissolveSettings Settings;Settings.DelaySeconds=0.f;Settings.DurationSeconds=.1f;
    TestFalse(TEXT("不轮询业务"),Dissolve->PrimaryComponentTick.bCanEverTick);
    TestTrue(TEXT("开始本代消散"),Dissolve->StartDissolve(1,Settings));
    TestFalse(TEXT("重复事件不重设截止"),Dissolve->StartDissolve(1,Settings));
    const FGuid FirstOperation=Dissolve->Operation;
    Dissolve->FinishDissolve(FirstOperation);
    TestFalse(TEXT("完成后隐藏主体"),Mesh->IsVisible());
    TestTrue(TEXT("同代次完成后仍去重"),Dissolve->IsDissolveActive());
    Dissolve->ResetDissolve();
    TestTrue(TEXT("重生恢复初始可见主体"),Mesh->IsVisible());
    TestFalse(TEXT("原先隐藏的Mesh保持隐藏"),HiddenMesh->IsVisible());
    TestTrue(TEXT("下一代重新开始"),Dissolve->StartDissolve(2,Settings));
    Dissolve->FinishDissolve(FirstOperation);
    TestTrue(TEXT("旧代截止不能隐藏新代主体"),Mesh->IsVisible());
    const FGuid SecondOperation=Dissolve->Operation;
    Dissolve->ResetDissolve();
    Dissolve->FinishDissolve(SecondOperation);
    TestTrue(TEXT("取消后的旧截止不能隐藏重生主体"),Mesh->IsVisible());
    TestTrue(TEXT("迟到死亡快照立即收敛隐藏"),Dissolve->StartDissolve(3,Settings,true));
    TestFalse(TEXT("迟到尸体不重播"),Mesh->IsVisible());
    Dissolve->DestroyComponent();
    TestTrue(TEXT("组件销毁归还可见性"),Mesh->IsVisible());
    auto* LongDelay=NewObject<UGamePlatformMeshDissolveComponent>(Actor);
    Actor->AddInstanceComponent(LongDelay);LongDelay->RegisterComponent();
    Settings.DelaySeconds=60.f;Settings.DurationSeconds=1.5f;
    TestTrue(TEXT("允许完整一分钟的表现等待"),LongDelay->StartDissolve(1,Settings));
    TestEqual(TEXT("材质消散起点在60秒后"),LongDelay->MaterialStartTime,float(World->GetTimeSeconds()+60.f));
    TestTrue(TEXT("等待时主体保留"),Mesh->IsVisible());
    LongDelay->ResetDissolve();Settings.DelaySeconds=60.01f;
    TestFalse(TEXT("超出有界延迟仍拒绝"),LongDelay->StartDissolve(2,Settings));
    LongDelay->DestroyComponent();
    Actor->Destroy();World->DestroyWorld(false);
    return true;
}
#endif
