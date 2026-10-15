// SSLM狼群外观由客户端从已授权内容包装配；权威AI/Combat与Fab第三方版权资产互不反向依赖。
#include "Creatures/DivineBeastsVillageWolfVisualWorldSubsystem.h"

#include "Characters/DivineBeastsVillageWolf.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Misc/ConfigCacheIni.h"
#include "Animation/AnimInstance.h"

bool UDivineBeastsVillageWolfVisualWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && !IsRunningCommandlet() && World->IsGameWorld() &&
        World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (bClosing || &InWorld != GetWorld() || InWorld.bIsTearingDown || SpawnHandle.IsValid()) return;
    Operation = FGuid::NewGuid();
    SpawnHandle = InWorld.AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this,&ThisClass::ObserveActor));
    for (TActorIterator<ADivineBeastsVillageWolf> It(&InWorld); It; ++It)
        ObserveActor(*It);
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::ObserveActor(AActor* Spawned)
{
    auto* Wolf = Cast<ADivineBeastsVillageWolf>(Spawned);
    UWorld* World = GetWorld();
    if (bClosing || !IsValid(Wolf) || Wolf->IsActorBeingDestroyed() || !World ||
        World->bIsTearingDown || Wolf->GetWorld()!=World || ObservedWolves.Contains(Wolf)) return;
    ObservedWolves.Add(Wolf);
    Wolf->OnEndPlay.AddUniqueDynamic(this,&ThisClass::HandleWolfEndPlay);
    if (bReady) ApplyVisual(*Wolf);
    else if (!bLoading) BeginLoadVisuals();
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::BeginLoadVisuals()
{
    // 未正式导入Fab或未审核时不替换客户端Mesh，不根据网络存在生成假狼外观。
    if (bClosing || bLoading || !GConfig) return;
    bool bVerified=false;
    const TCHAR* Section=TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section,TEXT("bClientVisualsVerified"),bVerified,GGameIni);
    if (!bVerified) return;
    FString MeshName, AnimName;
    GConfig->GetString(Section,TEXT("WolfSkeletalMesh"),MeshName,GGameIni);
    GConfig->GetString(Section,TEXT("WolfAnimationBlueprint"),AnimName,GGameIni);
    WolfMeshPath=FSoftObjectPath(MeshName);
    WolfAnimPath=FSoftObjectPath(AnimName);
    if (!WolfMeshPath.IsValid() || !WolfAnimPath.IsValid()) return;
    UWorld* World=GetWorld();
    if (!World || World->bIsTearingDown || !World->GetGameInstance()) return;
    auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance());
    if (!Data) return;
    bLoading=true;
    const FGuid Expected=Operation;
    TWeakObjectPtr<ThisClass> WeakThis(this);
    FGamePlatformResult Accepted;
    VisualLease=Data->AcquireResources({WolfMeshPath,WolfAnimPath},EGamePlatformDataLifetime::World,this,
        [WeakThis,Expected](const FGamePlatformDataLease& Lease,const FGamePlatformResult& Result)
        {
            auto* Self=WeakThis.Get();
            if (!Self || Self->bClosing || Self->Operation!=Expected ||
                !Self->VisualLease.IsValid() || Self->VisualLease.LeaseId!=Lease.LeaseId ||
                Self->VisualLease.ScopeId!=Lease.ScopeId || Self->VisualLease.Generation!=Lease.Generation ||
                Self->VisualLease.IssuerProof!=Lease.IssuerProof) return;
            Self->bLoading=false;
            UWorld* CurrentWorld=Self->GetWorld();
            if (!CurrentWorld || CurrentWorld->bIsTearingDown) return;
            USkeletalMesh* Asset=Cast<USkeletalMesh>(Self->WolfMeshPath.ResolveObject());
            UClass* Animation=Cast<UClass>(Self->WolfAnimPath.ResolveObject());
            Self->bReady=Result.IsSuccess() && IsValid(Asset) && IsValid(Animation) &&
                Animation->IsChildOf(UAnimInstance::StaticClass());
            if (!Self->bReady)
            {
                UE_LOG(LogTemp,Warning,TEXT("VillageWolfVisualRejected: 狼的Fab骨骼或动画蓝图资源尚未完成真实导入/校验。"));
                return;
            }
            const auto Wolves=Self->ObservedWolves.Array();
            for (const auto& WeakWolf:Wolves)
            {
                if (Self->bClosing || Self->Operation!=Expected || CurrentWorld->bIsTearingDown) break;
                if (auto* Wolf=WeakWolf.Get()) Self->ApplyVisual(*Wolf);
            }
        },Accepted);
    if (!Accepted.IsSuccess())
    {
        bLoading=false;
        UE_LOG(LogTemp,Warning,TEXT("VillageWolfVisualRequestDenied: GamePlatformData未接受狼外观租约请求。"));
    }
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::ApplyVisual(ADivineBeastsVillageWolf& Wolf)
{
    if (bClosing || !bReady || !ObservedWolves.Contains(&Wolf) || !IsValid(&Wolf) ||
        Wolf.IsActorBeingDestroyed() || Wolf.GetWorld()!=GetWorld() || !GetWorld() || GetWorld()->bIsTearingDown)
        return;
    auto* Mesh=Wolf.GetMesh();
    auto* Asset=Cast<USkeletalMesh>(WolfMeshPath.ResolveObject());
    auto* Animation=Cast<UClass>(WolfAnimPath.ResolveObject());
    if (!IsValid(Mesh) || !IsValid(Asset) || !IsValid(Animation) ||
        !Animation->IsChildOf(UAnimInstance::StaticClass())) return;
    // 客户端SkeletalMesh与AnimBP属于已通过平台租约审查的单一内容包；
    // 骨架轴向与模型比例在原生Editor验收后由导入Profile统一修订，绝不改写服务器胶囊。
    Mesh->SetSkeletalMeshAsset(Asset);
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetAnimInstanceClass(Animation);
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetRelativeLocation(FVector(0.0f,0.0f,-Wolf.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::HandleWolfEndPlay(AActor* Ended,EEndPlayReason::Type)
{
    auto* Wolf=Cast<ADivineBeastsVillageWolf>(Ended);
    if (!Wolf) return;
    Wolf->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleWolfEndPlay);
    ObservedWolves.Remove(Wolf);
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::Deinitialize()
{
    if (bClosing) return;
    bClosing=true;
    Operation.Invalidate();
    bReady=false; bLoading=false;
    UWorld* World=GetWorld();
    if (World && SpawnHandle.IsValid()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    SpawnHandle.Reset();
    const auto Old=MoveTemp(ObservedWolves);
    ObservedWolves.Reset();
    for (const auto& WeakWolf:Old)
        if (auto* Wolf=WeakWolf.Get()) Wolf->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleWolfEndPlay);
    const FGamePlatformDataLease OldLease=VisualLease;
    VisualLease={};
    if (World && World->GetGameInstance())
        if (auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance()); Data && OldLease.IsValid())
            Data->ReleaseResources(OldLease);
    Super::Deinitialize();
}
