// 项目专服狼装配回归：真实Game世界、平台AI注册器和狼实体，验证手动占有不被抢占。
// 只测试控制器/Combat初始生命；不登录、不伪造AI资源就绪或替代专服生成/联机验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "Village/DivineBeastsVillageWolfAIController.h"
#include "Controllers/GamePlatformAIController.h"
#include "Perception/GamePlatformAITargetRegistrySubsystem.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Tags/GamePlatformCombatTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVillageWolfSingleControllerInitializationTest,
    "DivineBeasts.Server.VillageWolf.SingleControllerInitialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FVillageWolfSingleControllerInitializationTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    if(!TestNotNull(TEXT("真实权威Game测试世界"),World))return false;
    ON_SCOPE_EXIT{World->DestroyWorld(false);};
    World->InitializeActorsForPlay(FURL());
    TestNotNull(TEXT("真实平台AI注册器已装配"),World->GetSubsystem<UGamePlatformAITargetRegistrySubsystem>());
    auto* Wolf=World->SpawnActor<ADivineBeastsVillageWolf>();
    if(!TestNotNull(TEXT("真实狼实体"),Wolf))return false;
    TestNull(TEXT("禁用自动占有的狼不得被平台提前生成通用控制器"),Wolf->GetController());
    Wolf->DispatchBeginPlay();
    auto* Controller=World->SpawnActor<ADivineBeastsVillageWolfAIController>();
    if(!TestNotNull(TEXT("项目唯一狼控制器"),Controller))return false;
    Controller->Possess(Wolf);
    TestEqual(TEXT("项目控制器保持占有"),Wolf->GetController(),static_cast<AController*>(Controller));
    TestNull(TEXT("控制器Owner不能反向指向Pawn而构成所有权环"),Controller->GetOwner());
    TestTrue(TEXT("占有后真实Combat首次初始化成功"),Wolf->AuthorityInitializeWolf(100.0f));
    TestEqual(TEXT("权威生命为100"),Wolf->GetGamePlatformCombatComponent()->GetCombatHealth(),100.0f);
    // 真正AI控制器消费同一个GAS控制事件；此处不伪造导航请求或宣称路径实机验收。
    auto* Source=World->SpawnActor<AActor>();
    auto* SourceASC=NewObject<UGamePlatformAbilitySystemComponent>(Source);
    auto* SourceCombat=NewObject<UGamePlatformCombatComponent>(Source);
    Source->AddInstanceComponent(SourceASC); Source->AddInstanceComponent(SourceCombat);
    SourceASC->RegisterComponent(); SourceCombat->RegisterComponent();
    SourceASC->InitAbilityActorInfo(Source,Source);
    Source->DispatchBeginPlay();
    FGamePlatformCombatSpec Control;
    Control.EventId=FGuid::NewGuid(); Control.Source=Source; Control.Target=Wolf;
    Control.SourceAvatarGeneration=SourceCombat->GetCombatAvatarGeneration();
    Control.TargetAvatarGeneration=Wolf->GetCombatAvatarGeneration();
    TestTrue(TEXT("真实GAS控制施加"),SourceCombat->ApplyControl(Control,EGamePlatformControlType::Stun,1.f).IsSuccess());
    TestEqual(TEXT("怪物AI收到控制事件并进入Controlled"),Wolf->GetWolfAIState()->GetSnapshot().PublicState,EGamePlatformAIPublicState::Controlled);
    TestTrue(TEXT("控制期间真实Stun标签有效"),Wolf->GetGamePlatformAbilitySystemComponent()->HasMatchingGameplayTag(GamePlatformCombatTags::Control_Stun));
    TestTrue(TEXT("平台可撤销同一个控制效果"),Wolf->GetGamePlatformCombatComponent()->RemoveControl(EGamePlatformControlType::Stun));
    TestFalse(TEXT("解除控制不残留标签"),Wolf->GetGamePlatformAbilitySystemComponent()->HasMatchingGameplayTag(GamePlatformCombatTags::Control_Stun));
    int32 ControllerCount=0;
    for(TActorIterator<AGamePlatformAIController> It(World);It;++It)++ControllerCount;
    TestEqual(TEXT("世界中只有一套狼AI控制器"),ControllerCount,1);
    return true;
}
#endif
