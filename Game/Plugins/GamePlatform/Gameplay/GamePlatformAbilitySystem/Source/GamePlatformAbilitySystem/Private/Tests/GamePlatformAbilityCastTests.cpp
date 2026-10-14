// 平台施法事实的真实组件/世界回归：不伪造网络或项目魔法资源。
// 反射调用公开API使第一轮可在类型尚未实现时编译并得到真实RED；测试不向生产类添加测试接口。
// 验证权威发布、身份匹配取消、有界时间和真实Timer终态；不把单机世界等同联机复制验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace GamePlatformAbilityCastTests
{
/** 独占无物理训练世界，组件由Actor拥有；销毁世界取消所有本夹具计时器。 */
struct FFixture
{
    UWorld* World = nullptr;
    TStrongObjectPtr<UWorld> WorldLifetime;
    UActorComponent* Component = nullptr;
    FFixture(UClass* Class)
    {
        const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false)
            .RequiresHitProxies(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        // CreateWorld已初始化；不重复InitializeNewWorld造成双重子系统/世界生命周期。
        World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
        WorldLifetime.Reset(World);
        // 真实World::Tick会查询SeamlessTravelHandler；必须登记/释放独占引擎上下文，不能只有UWorld对象。
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        AActor* Owner = World->SpawnActor<AActor>();
        Component = NewObject<UActorComponent>(Owner, Class);
        Owner->AddInstanceComponent(Component); Component->RegisterComponent();
        Owner->DispatchBeginPlay();
        World->BeginPlay();
    }
    ~FFixture() { if(World) { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); } }
    /** 只调用生产公开UFUNCTION，输入值与期望值独立构造。 */
    bool Publish(FGuid Id, float Duration)
    {
        UFunction* Function = Component->FindFunction(TEXT("PublishConfirmedCast"));
        if(!Function) return false;
        FStructOnScope Args(Function); uint8* Data = Args.GetStructMemory();
        *FindFProperty<FStructProperty>(Function, TEXT("ActivationId"))->ContainerPtrToValuePtr<FGuid>(Data) = Id;
        FindFProperty<FNameProperty>(Function,TEXT("AbilityId"))->SetPropertyValue_InContainer(Data,TEXT("test.ability@1"));
        FindFProperty<FNameProperty>(Function,TEXT("PresentationStyleId"))->SetPropertyValue_InContainer(Data,TEXT("test.cast.style"));
        FindFProperty<FIntProperty>(Function,TEXT("AvatarGeneration"))->SetPropertyValue_InContainer(Data,1);
        FindFProperty<FFloatProperty>(Function,TEXT("DurationSeconds"))->SetPropertyValue_InContainer(Data,Duration);
        Component->ProcessEvent(Function,Data);
        return FindFProperty<FBoolProperty>(Function,TEXT("ReturnValue"))->GetPropertyValue_InContainer(Data);
    }
    bool Cancel(FGuid Id)
    {
        UFunction* Function = Component->FindFunction(TEXT("CancelConfirmedCast"));
        if(!Function) return false;
        FStructOnScope Args(Function); uint8* Data=Args.GetStructMemory();
        *FindFProperty<FStructProperty>(Function,TEXT("ActivationId"))->ContainerPtrToValuePtr<FGuid>(Data)=Id;
        Component->ProcessEvent(Function,Data);
        return FindFProperty<FBoolProperty>(Function,TEXT("ReturnValue"))->GetPropertyValue_InContainer(Data);
    }
    bool Active() const
    {
        UFunction* Function=Component->FindFunction(TEXT("GetCastFacts"));
        if(!Function) return false;
        FStructOnScope Args(Function); Component->ProcessEvent(Function,Args.GetStructMemory());
        FStructProperty* Return=FindFProperty<FStructProperty>(Function,TEXT("ReturnValue"));
        return FindFProperty<FBoolProperty>(Return->Struct,TEXT("bActive"))->GetPropertyValue_InContainer(Return->ContainerPtrToValuePtr<void>(Args.GetStructMemory()));
    }
};

/** TimerManager每个引擎帧只执行一次；潜伏命令推进隔离World，不修改GFrameCounter伪造计时。 */
class FExpiryCommand final : public IAutomationLatentCommand
{
public:
    FExpiryCommand(FAutomationTestBase* InTest,UClass* Class): Test(InTest),Fixture(Class) {}
    bool Update() override
    {
        if (Frame==0) { Test->TestTrue(TEXT("新动作可重新发布"),Fixture.Publish(FGuid::NewGuid(),0.1f)); ++Frame; return false; }
        Fixture.World->Tick(LEVELTICK_All,0.05f); ++Frame;
        if (Frame==2) Test->TestTrue(TEXT("截止前保持活跃"),Fixture.Active());
        if (Frame<5) return false;
        Test->TestFalse(TEXT("真实计时器到期清活跃态"),Fixture.Active()); return true;
    }
private:
    FAutomationTestBase* Test;
    FFixture Fixture;
    int32 Frame=0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformAbilityCastPublicationTest,
    "GamePlatform.AbilitySystem.Cast.AuthorityIdentityAndExpiry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformAbilityCastPublicationTest::RunTest(const FString&)
{
    UClass* Class=FindObject<UClass>(nullptr,TEXT("/Script/GamePlatformAbilitySystem.GamePlatformAbilityCastComponent"));
    if(!TestNotNull(TEXT("平台施法事实组件必须由真实模块编译注册"),Class)) return false;
    GamePlatformAbilityCastTests::FFixture Fixture(Class);
    const FGuid First=FGuid::NewGuid(), Next=FGuid::NewGuid();
    TestFalse(TEXT("零身份不能发布有效事实"),Fixture.Publish({},0.5f));
    TestFalse(TEXT("零时长拒绝"),Fixture.Publish(First,0.0f));
    TestFalse(TEXT("超出十秒预算拒绝"),Fixture.Publish(First,11.0f));
    TestTrue(TEXT("合法权威施法发布"),Fixture.Publish(First,0.5f));
    TestTrue(TEXT("消费者看到活跃事实"),Fixture.Active());
    TestFalse(TEXT("重复身份不能重置原始开始时间"),Fixture.Publish(First,0.5f));
    TestFalse(TEXT("旧取消身份不能清新动作"),Fixture.Cancel(Next));
    TestTrue(TEXT("后继动作原子替换旧事实"),Fixture.Publish(Next,0.5f));
    TestFalse(TEXT("前一动作取消不影响后继"),Fixture.Cancel(First));
    TestTrue(TEXT("匹配取消成功"),Fixture.Cancel(Next));
    TestFalse(TEXT("取消后不再活跃"),Fixture.Active());
    ADD_LATENT_AUTOMATION_COMMAND(GamePlatformAbilityCastTests::FExpiryCommand(this,Class));
    return true;
}
#endif
