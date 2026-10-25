// 项目客户端死亡表现回归：以真实复制字段通知驱动既有组件，不提交伤害、不连接后端、不保存资产。
// 狼与吸血鬼必须保留三秒，再经历原有1.5秒消散；复活取消、重复通知及普通角色原规则分别验证。
#include "Characters/DivineBeastsDeathDissolveComponent.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Characters/DivineBeastsVillageVampire.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMonsterDeathDissolveTimingTest,
    "DivineBeasts.Presentation.MonsterDeath.ThreeSecondHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMonsterDeathDissolveTimingTest::RunTest(const FString&)
{
    const auto Initialization=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,
        ERHIFeatureLevel::Num,&Initialization);
    if(!TestNotNull(TEXT("隔离真实世界"),World))return false;
    // TimerManager每引擎帧只推进一次；同步测试显式推进帧号，离开时归还，不运行其它World的Tick。
    const uint64 OriginalFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->DestroyWorld(false); GFrameCounter=OriginalFrame; };
    const auto Advance=[World](float Seconds){++GFrameCounter;World->GetTimerManager().Tick(Seconds);};
    Advance(0.f);
    const FBoolProperty* Dead=FindFProperty<FBoolProperty>(UGamePlatformCombatComponent::StaticClass(),TEXT("bDead"));
    if(!TestNotNull(TEXT("真实复制死亡字段"),Dead))return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for(UClass* MonsterClass:{ADivineBeastsVillageWolf::StaticClass(),ADivineBeastsVillageVampire::StaticClass()})
    {
        auto* Monster=World->SpawnActor<ADivineBeastsVillageWolf>(MonsterClass,FTransform::Identity,Spawn);
        if(!TestNotNull(TEXT("真实怪物类型"),Monster))return false;
        auto* Combat=Monster->GetGamePlatformCombatComponent();
        auto* Mesh=Monster->GetMesh();Mesh->SetVisibility(true,false);
        auto* Dissolve=NewObject<UDivineBeastsDeathDissolveComponent>(Monster);
        Monster->AddInstanceComponent(Dissolve);Dissolve->RegisterComponent();Dissolve->BeginPlay();
        // 复制夹具仅改变客户端已观察事实；不调用权威伤害/死亡、重生或角色初始化。
        const auto Notify=[Dead,Combat](bool bDead)
        {Dead->SetPropertyValue_InContainer(Combat,bDead);Combat->OnCombatObservationChanged().Broadcast();};
        Notify(true);
        TestTrue(TEXT("死亡观察启动同一消散组件"),Dissolve->IsDissolveActive());
        Advance(2.99f);
        TestTrue(TEXT("死亡后2.99秒尸体仍完整可见"),Mesh->IsVisible());
        // 同代重复观察不得重设起点，使消散在4.5秒总窗口内正常结束。
        Notify(true);Advance(.02f);
        TestTrue(TEXT("3秒后开始消散而非立刻隐藏"),Mesh->IsVisible());
        Advance(1.48f);
        TestTrue(TEXT("4.49秒仍在消散窗口"),Mesh->IsVisible());
        Advance(.02f);
        TestFalse(TEXT("4.51秒完成隐藏且重复事件没有延长截止"),Mesh->IsVisible());
        Notify(false);
        TestTrue(TEXT("复活恢复初始可见性"),Mesh->IsVisible());
        Notify(true);Advance(1.f);Notify(false);Advance(5.f);
        TestTrue(TEXT("等待中复活取消旧截止"),Mesh->IsVisible());
        Monster->Destroy();
    }
    // 普通角色不是本次怪物参数修改的目标，继续沿用平台0.65+1.5秒默认。
    ACharacter* Character=World->SpawnActor<ACharacter>(Spawn);
    auto* Combat=NewObject<UGamePlatformCombatComponent>(Character);
    Character->AddInstanceComponent(Combat);Combat->RegisterComponent();
    auto* Dissolve=NewObject<UDivineBeastsDeathDissolveComponent>(Character);
    Character->AddInstanceComponent(Dissolve);Dissolve->RegisterComponent();Dissolve->BeginPlay();
    Dead->SetPropertyValue_InContainer(Combat,true);Combat->OnCombatObservationChanged().Broadcast();
    Advance(2.14f);TestTrue(TEXT("普通角色保留原截止"),Character->GetMesh()->IsVisible());
    Advance(.02f);TestFalse(TEXT("普通角色未被改成三秒等待"),Character->GetMesh()->IsVisible());
    Character->Destroy();
    return true;
}
#endif
