#include "Development/DivineBeastsProjectileMotionDemo.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

ADivineBeastsProjectileMotionDemo::ADivineBeastsProjectileMotionDemo()
{
    PrimaryActorTick.bCanEverTick=false;
    bReplicates=true;
    auto* Root=CreateDefaultSubobject<USphereComponent>(TEXT("DemoRoot"));SetRootComponent(Root);
    Root->InitSphereRadius(30);Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AbilitySystem=CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>(TEXT("DemoAbilitySystem"));
    Combat=CreateDefaultSubobject<UGamePlatformCombatComponent>(TEXT("DemoCombat"));
}
void ADivineBeastsProjectileMotionDemo::BeginPlay()
{
    AbilitySystem->InitAbilityActorInfo(this,this);Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if(HasAuthority() && GetNetMode()!=NM_Client && FlightProfile && GetWorld()->IsGameWorld())
        GetWorldTimerManager().SetTimer(LaunchTimer,this,&ADivineBeastsProjectileMotionDemo::LaunchDemo,3.f,true,.5f);
#endif
}
void ADivineBeastsProjectileMotionDemo::LaunchDemo()
{
#if !UE_BUILD_SHIPPING
    if(!HasAuthority() || GetNetMode()==NM_Client || !FlightProfile || !GetWorld() || !GetWorld()->IsGameWorld() || GetWorld()->bIsTearingDown)return;
    Projectiles.RemoveAll([](const auto& P){return !P.IsValid();});
    if(Projectiles.Num()>=4)return;
    FGamePlatformCombatSpec Spec;Spec.EventId=FGuid::NewGuid();Spec.Source=this;
    Spec.SourceAvatarGeneration=Combat->GetCombatAvatarGeneration();Spec.Magnitude=1;Spec.SourceAbilityId=TEXT("Ability.Demo.ProjectileMotion");
    if(IsValid(Target) && FlightProfile->Flight.bEnableHoming)
    {
        if(const auto* C=Target->FindComponentByClass<UGamePlatformCombatComponent>())
        {Spec.Target=Target;Spec.TargetAvatarGeneration=C->GetCombatAvatarGeneration();}
    }
    FGamePlatformCombatProjectileMotion Motion;Motion.RangeCm=4000;Motion.FlightProfile=FlightProfile;
    auto* P=GetWorld()->SpawnActor<AGamePlatformCombatProjectile>(GetActorLocation()+LaunchDirection.GetSafeNormal()*50,LaunchDirection.Rotation());
    if(!P)return;
    if(!P->AuthorityInitialize(Spec,Motion,LaunchDirection,TEXT("DBA.Development.Fireball"))){P->Destroy();return;}
    Projectiles.Add(P);
#if !UE_SERVER
    if(bShowDebugMesh && GetNetMode()!=NM_DedicatedServer)
    {
        // 只用于此开发夹具的通用运动代理；与正式Niagara资源无绑定。
        if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")))
        {
            auto* Visual=NewObject<UStaticMeshComponent>(P);P->AddInstanceComponent(Visual);
            Visual->SetStaticMesh(Mesh);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Visual->SetupAttachment(P->GetRootComponent());Visual->SetRelativeScale3D(FVector(.25));Visual->RegisterComponent();
        }
    }
#endif
#endif
}
void ADivineBeastsProjectileMotionDemo::EndPlay(EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(LaunchTimer);
    for(auto& Weak:Projectiles)if(auto* P=Weak.Get()){P->AuthorityCancel();P->Destroy();}
    Projectiles.Reset();Super::EndPlay(Reason);
}
