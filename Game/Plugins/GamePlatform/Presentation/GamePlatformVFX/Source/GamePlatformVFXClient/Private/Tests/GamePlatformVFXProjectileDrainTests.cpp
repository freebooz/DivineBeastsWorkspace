// 原生生命周期回归；真实Niagara粒子/灯光/艺术质量另由资产与RHI门禁验证。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Definitions/GamePlatformVFXProjectileDefinition.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SceneComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXProjectileDrainSourceTest,
 "GamePlatform.VFX.ProjectileDrain.SurvivesSourceDestroy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXProjectileDrainSourceTest::RunTest(const FString&)
{
 const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values));
 const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> Service(NewObject<UGamePlatformVFXWorldSubsystem>(World.Get()));
 const TStrongObjectPtr<UGamePlatformVFXProjectileDefinition> Definition(NewObject<UGamePlatformVFXProjectileDefinition>());
 UNiagaraSystem* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_RainSplash.NS_GP_Weather_RainSplash"));
 TestNotNull(TEXT("真实平台测试System存在"),System);
 auto* Property=FindFProperty<FSoftObjectProperty>(UGamePlatformVFXDefinition::StaticClass(),TEXT("NiagaraSystem"));
 *Property->ContainerPtrToValuePtr<TSoftObjectPtr<UNiagaraSystem>>(Definition.Get())=System;
 AActor* Source=World->SpawnActor<AActor>();
 USceneComponent* Root=NewObject<USceneComponent>(Source);Source->AddInstanceComponent(Root);Source->SetRootComponent(Root);Root->RegisterComponent();
 FGamePlatformVFXRequest Request;Request.SpawnContext.AttachComponent=Root;
 UNiagaraComponent* Component=FGamePlatformVFXNiagaraExecutor::Spawn(*World,*Definition,Request,false);
 if(!TestNotNull(TEXT("生产Executor生成组件"),Component)){World->DestroyWorld(false);return false;}
 TestTrue(TEXT("组件由WorldSettings而非来源Actor拥有"),Component->GetOwner()==World->GetWorldSettings());
 TestTrue(TEXT("飞行时仍附着来源"),Component->GetAttachParent()==Root);
 Source->SetActorLocation(FVector(200.,10.,50.));Source->SetActorRotation(FRotator(0.,75.,0.));
 TestTrue(TEXT("生产组件随来源真实移动"),Component->GetComponentLocation().Equals(Source->GetActorLocation(),.01));
 TestTrue(TEXT("生产组件随来源真实转向"),Component->GetComponentRotation().Equals(Source->GetActorRotation(),.01));
 auto Handle=Service->InstanceRegistry.Reserve(World.Get());Service->InstanceRegistry.SetDefinition(Handle,Definition.Get());Service->InstanceRegistry.AttachComponent(Handle,Component,false);
 FGamePlatformVFXPlaybackSnapshot Snapshot;Snapshot.Handle=Handle;Snapshot.State=EGamePlatformVFXPlaybackState::Playing;Service->PlaybackSnapshots.Add(Handle.Id,Snapshot);
 Component->SetActiveFlag(true);
 TestTrue(TEXT("确认结束可进入Drain"),Service->StopWithMode(Handle,EGamePlatformVFXStopMode::Drain));
 const FTransform Frozen=Component->GetComponentTransform();Source->SetActorLocation(FVector(5000.,0.,0.));
 TestTrue(TEXT("Drain尾迹保持世界变换，不再跟随来源"),Component->GetComponentTransform().Equals(Frozen,.01));
 Source->Destroy();
 TestTrue(TEXT("来源销毁不销毁世界尾迹组件"),IsValid(Component));
 TestNull(TEXT("Drain解除来源附着"),Component->GetAttachParent());
 TestTrue(TEXT("保留停止中账本直到真实结束"),Service->InstanceRegistry.IsStopping(Handle));
 TestTrue(TEXT("重复Drain不重建生命周期"),Service->StopWithMode(Handle,EGamePlatformVFXStopMode::Drain));
 TestTrue(TEXT("停止中记录有兜底计时器"),Service->LifetimeTimers.Contains(Handle.Id));
 TestTrue(TEXT("兜底时间有界"),World->GetTimerManager().GetTimerRemaining(Service->LifetimeTimers.FindChecked(Handle.Id))<=.5f);
 Service->Stop(Handle);
 TestFalse(TEXT("显式取消立即清理停止中记录"),Service->InstanceRegistry.OwnsHandle(Handle));
 TestFalse(TEXT("显式取消清理计时器"),Service->LifetimeTimers.Contains(Handle.Id));
 Service->Deinitialize();World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXProjectileDrainExitTest,
 "GamePlatform.VFX.ProjectileDrain.WorldExitCancelsDrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXProjectileDrainExitTest::RunTest(const FString&)
{
 const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));
 const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> Service(NewObject<UGamePlatformVFXWorldSubsystem>(World.Get()));
 const TStrongObjectPtr<UGamePlatformVFXProjectileDefinition> Definition(NewObject<UGamePlatformVFXProjectileDefinition>());
 UNiagaraComponent* Component=NewObject<UNiagaraComponent>(World.Get());Component->SetActiveFlag(true);
 auto Handle=Service->InstanceRegistry.Reserve(World.Get());Service->InstanceRegistry.SetDefinition(Handle,Definition.Get());Service->InstanceRegistry.AttachComponent(Handle,Component,false);
 FGamePlatformVFXPlaybackSnapshot Snapshot;Snapshot.Handle=Handle;Snapshot.State=EGamePlatformVFXPlaybackState::Playing;Service->PlaybackSnapshots.Add(Handle.Id,Snapshot);
 TestTrue(TEXT("进入停止中"),Service->StopWithMode(Handle,EGamePlatformVFXStopMode::Drain));
 Service->Deinitialize();
 TestEqual(TEXT("World退出立即清空尾迹账本"),Service->InstanceRegistry.Num(),0);
 TestEqual(TEXT("World退出清空兜底定时器"),Service->LifetimeTimers.Num(),0);
 TestEqual(TEXT("World退出形成独立终态"),Service->GetPlaybackSnapshot(Handle).State,EGamePlatformVFXPlaybackState::WorldDestroyed);
 World->GetTimerManager().Tick(.6f);
 TestEqual(TEXT("迟到超时不得复活账本"),Service->InstanceRegistry.Num(),0);
 World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXProjectileDrainStaleTest,
 "GamePlatform.VFX.ProjectileDrain.StaleHandleCannotStopReusedComponent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXProjectileDrainStaleTest::RunTest(const FString&)
{
 const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));
 const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> Service(NewObject<UGamePlatformVFXWorldSubsystem>(World.Get()));
 const TStrongObjectPtr<UGamePlatformVFXProjectileDefinition> Definition(NewObject<UGamePlatformVFXProjectileDefinition>());
 UNiagaraComponent* Component=NewObject<UNiagaraComponent>(World.Get());Component->SetActiveFlag(true);
 auto Old=Service->InstanceRegistry.Reserve(World.Get());auto Current=Service->InstanceRegistry.Reserve(World.Get());
 for(auto Handle:{Old,Current}){Service->InstanceRegistry.SetDefinition(Handle,Definition.Get());Service->InstanceRegistry.AttachComponent(Handle,Component,false);FGamePlatformVFXPlaybackSnapshot Snapshot;Snapshot.Handle=Handle;Snapshot.State=EGamePlatformVFXPlaybackState::Playing;Service->PlaybackSnapshots.Add(Handle.Id,Snapshot);}
 auto Forged=Current;++Forged.Generation;
 TestFalse(TEXT("代次不符不能Drain"),Service->StopWithMode(Forged,EGamePlatformVFXStopMode::Drain));
 Service->StopWithMode(Old,EGamePlatformVFXStopMode::Drain);
 TestTrue(TEXT("旧账本不能取消新Owner活跃状态"),Component->IsActive());
 TestEqual(TEXT("反向索引仍指向新完整句柄"),Service->InstanceRegistry.FindByComponent(Component),Current);
 TestFalse(TEXT("旧Owner不得给复用组件安装Drain"),Service->InstanceRegistry.IsStopping(Old));
 Service->Deinitialize();World->DestroyWorld(false);return true;
}
#endif
