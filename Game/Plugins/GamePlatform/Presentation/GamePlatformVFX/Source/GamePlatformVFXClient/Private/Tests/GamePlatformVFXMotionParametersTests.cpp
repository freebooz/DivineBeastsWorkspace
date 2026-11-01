// 真实Niagara参数存储回归；无战斗/项目依赖，不激活GPU模拟。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Interfaces/GamePlatformMotionSource.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Tests/GamePlatformVFXMotionTestSource.h"
#include "Instances/GamePlatformVFXInstanceRegistry.h"
#include "Definitions/GamePlatformVFXProjectileDefinition.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXMotionParametersTest,
    "GamePlatform.VFX.ProjectileMotion.DeclaredParametersOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXMotionParametersTest::RunTest(const FString&)
{
    TStrongObjectPtr<UNiagaraSystem> System(NewObject<UNiagaraSystem>());
    System->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.ProjectileSpeed")));
    System->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.FlightDirection")));
    TStrongObjectPtr<UNiagaraComponent> Component(NewObject<UNiagaraComponent>());Component->SetAsset(System.Get());
    FGamePlatformMotionSnapshot S;S.bValid=true;S.bEnableSpeedBasedVFX=true;S.Speed=1200;S.MaxSpeed=1800;
    S.SpeedNormalized=2.f/3;S.AccelerationNormalized=1;S.FlightDirection=FVector::RightVector;
    TestEqual(TEXT("只写声明的Float与Vector"),FGamePlatformVFXNiagaraExecutor::ApplyMotionParameters(*Component,S),2);
    bool bValid=false;
    TestEqual(TEXT("真实OverrideStore速度"),Component->GetVariableFloat(TEXT("User.ProjectileSpeed"),bValid),1200.f);
    TestTrue(TEXT("速度读取有效"),bValid);
    TestEqual(TEXT("真实OverrideStore方向"),Component->GetVariableVec3(TEXT("User.FlightDirection"),bValid),FVector::RightVector);
    TestTrue(TEXT("方向读取有效"),bValid);
    TestEqual(TEXT("未创建未声明的归一化速度"),Component->GetOverrideParameters().IndexOf(
        FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.SpeedNormalized"))),INDEX_NONE);
    S.bEnableSpeedBasedVFX=false;
    TestEqual(TEXT("独立关闭视觉联动"),FGamePlatformVFXNiagaraExecutor::ApplyMotionParameters(*Component,S),0);
    S.bEnableSpeedBasedVFX=true;S.Speed=NAN;
    TestEqual(TEXT("非法样本不污染Niagara"),FGamePlatformVFXNiagaraExecutor::ApplyMotionParameters(*Component,S),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXMotionQueryReentryTest,
    "GamePlatform.VFX.ProjectileMotion.QueryReentryCannotWriteReusedComponent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXMotionQueryReentryTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init));
    if(!World.IsValid())return false;
    World->InitializeActorsForPlay(FURL());
    auto* Source=World->SpawnActor<AGamePlatformVFXMotionTestSource>();
    Source->Sample.bValid=true;Source->Sample.bEnableSpeedBasedVFX=true;Source->Sample.Speed=1200;
    auto* System=NewObject<UNiagaraSystem>();System->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User.ProjectileSpeed")));
    auto* Component=NewObject<UNiagaraComponent>(World->GetWorldSettings());
    Component->SetAutoActivate(false);Component->SetAutoDestroy(false);Component->SetAsset(System);Component->RegisterComponentWithWorld(World.Get());
    TestTrue(TEXT("夹具真实附着到运动来源"),Component->AttachToComponent(Source->GetRootComponent(),FAttachmentTransformRules::KeepWorldTransform));
    auto* Definition=NewObject<UGamePlatformVFXProjectileDefinition>();
    FGamePlatformVFXInstanceRegistry Registry;const auto Old=Registry.Reserve(World.Get());
    TestTrue(TEXT("夹具定义为Projectile"),Definition->GetBehavior()==EGamePlatformVFXBehavior::Projectile);
    TestTrue(TEXT("夹具来源实现中立运动接口"),Source->GetClass()->ImplementsInterface(UGamePlatformMotionSource::StaticClass()));
    TestTrue(TEXT("夹具来源和组件同世界"),Source->GetWorld()==Component->GetWorld());
    TestTrue(TEXT("夹具组件有效"),IsValid(Component));
    TestTrue(TEXT("夹具保存Definition"),Registry.SetDefinition(Old,Definition));
    TestTrue(TEXT("夹具保存Component"),Registry.AttachComponent(Old,Component,false));
    TestTrue(TEXT("夹具启用运动绑定"),Registry.SetMotionBindingEnabled(Old,true));
    TestTrue(TEXT("夹具反向Owner有效"),Registry.FindByComponent(Component)==Old);
    TestTrue(TEXT("夹具执行接口可读真实样本"),IGamePlatformMotionSource::Execute_GetMotionSnapshot(Source).bValid);
    FGamePlatformVFXHandle Successor;
    bool bQueried=false;
    Source->OnQuery=[&]()
    {
        if(bQueried)return;
        bQueried=true;
        Registry.Stop(Old,false);Successor=Registry.Reserve(World.Get());Registry.SetDefinition(Successor,Definition);
        Registry.AttachComponent(Successor,Component,false);Registry.SetMotionBindingEnabled(Successor,true);
        Component->SetVariableFloat(TEXT("User.ProjectileSpeed"),1700);
    };
    TestEqual(TEXT("查询后新Owner仍被世界上传跟踪"),Registry.UpdateProjectileMotionParameters(),1);
    TestTrue(TEXT("必须实际执行可重入运动查询"),bQueried);
    bool bValid=false;TestEqual(TEXT("旧查询不能覆盖新Owner的1700速度"),Component->GetVariableFloat(TEXT("User.ProjectileSpeed"),bValid),1700.f);
    TestTrue(TEXT("新完整句柄仍拥有组件"),Registry.FindByComponent(Component)==Successor);
    Source->Sample.Speed=1700;
    TestEqual(TEXT("后续帧继续更新新Owner"),Registry.UpdateProjectileMotionParameters(),1);
    TestEqual(TEXT("后续帧保持正确速度"),Component->GetVariableFloat(TEXT("User.ProjectileSpeed"),bValid),1700.f);
    Source->OnQuery=nullptr;Registry.Stop(Successor,false);World->DestroyWorld(false);
    return true;
}
#endif
