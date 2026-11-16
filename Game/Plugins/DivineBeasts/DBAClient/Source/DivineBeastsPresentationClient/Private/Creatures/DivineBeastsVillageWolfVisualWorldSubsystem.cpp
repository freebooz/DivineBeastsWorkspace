// SSLM狼群外观由客户端从已授权内容包装配；权威AI/Combat不依赖第三方模型的作者或资源格式。
#include "Creatures/DivineBeastsVillageWolfVisualWorldSubsystem.h"

#include "Characters/DivineBeastsVillageWolf.h"
#include "Characters/DivineBeastsVillageVampire.h"
#include "Characters/DivineBeastsDeathDissolveComponent.h"
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
#include "Animation/AnimBlueprintGeneratedClass.h"

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
    const bool bVampire = Wolf->IsA<ADivineBeastsVillageVampire>();
    const auto& Visual = bVampire ? VampireVisual : WolfVisual;
    if (Visual.bReady) ApplyVisual(*Wolf);
    else if (!Visual.bLoading) BeginLoadVisuals(bVampire);
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::BeginLoadVisuals(bool bVampire)
{
    // 每种怪物均需显式通过开发资产门禁；同世界中只申请各自的单份Data租约。
    FCreatureVisual& Visual = bVampire ? VampireVisual : WolfVisual;
    if (bClosing || Visual.bLoading || Visual.bReady || Visual.bHasLoadAttempted || !GConfig) return;
    bool bVerified=false;
    const TCHAR* Section=TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section,bVampire ? TEXT("bVampireClientVisualsVerified") :
        TEXT("bClientVisualsVerified"),bVerified,GGameIni);
    if (!bVerified) return;
    GConfig->GetFloat(Section,bVampire ? TEXT("VampireMeshScale") : TEXT("WolfMeshScale"),Visual.Scale,GGameIni);
    GConfig->GetFloat(Section,bVampire ? TEXT("VampireMeshYawDegrees") :
        TEXT("WolfMeshYawDegrees"),Visual.YawDegrees,GGameIni);
    if(!FMath::IsFinite(Visual.Scale)||Visual.Scale<0.01f||Visual.Scale>10.0f||
        !FMath::IsFinite(Visual.YawDegrees)||FMath::Abs(Visual.YawDegrees)>360.0f)
    {
        UE_LOG(LogTemp,Warning,TEXT("VillageCreatureVisualProfileRejected: 模型比例或朝向无效。"));
        return;
    }
    FString MeshName, AnimName;
    GConfig->GetString(Section,bVampire ? TEXT("VampireSkeletalMesh") :
        TEXT("WolfSkeletalMesh"),MeshName,GGameIni);
    GConfig->GetString(Section,bVampire ? TEXT("VampireAnimationBlueprint") :
        TEXT("WolfAnimationBlueprint"),AnimName,GGameIni);
    Visual.MeshPath=FSoftObjectPath(MeshName);
    Visual.AnimPath=FSoftObjectPath(AnimName);
    // 狼继续使用本地已审核的动物内容包归属；吸血鬼保留独立的已许可外观路径。
    const FString AnimalContentRoot(TEXT("/DBAContentPack_AnimalsProtofactor/"));
    if (!Visual.MeshPath.IsValid() || !Visual.AnimPath.IsValid() ||
        (!bVampire && (!Visual.MeshPath.GetLongPackageName().StartsWith(AnimalContentRoot) ||
            !Visual.AnimPath.GetLongPackageName().StartsWith(AnimalContentRoot))))
    {
        if (!Visual.bHasLoggedMissingAssets)
            UE_LOG(LogTemp, Warning, TEXT("VillageCreatureVisualConfigMissing: 怪物外观路径或已审核内容包归属无效。"));
        Visual.bHasLoggedMissingAssets = true;
        return;
    }
    UWorld* World=GetWorld();
    if (!World || World->bIsTearingDown || !World->GetGameInstance()) return;
    auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance());
    if (!Data) return;
    Visual.bHasLoadAttempted = true;
    Visual.bLoading=true;
    const FGuid Expected=Operation;
    TWeakObjectPtr<ThisClass> WeakThis(this);
    FGamePlatformResult Accepted;
    Visual.Lease=Data->AcquireResources({Visual.MeshPath,Visual.AnimPath},EGamePlatformDataLifetime::World,this,
        [WeakThis,Expected,bVampire](const FGamePlatformDataLease& Lease,const FGamePlatformResult& Result)
        {
            auto* Self=WeakThis.Get();
            if (!Self) return;
            FCreatureVisual& CurrentVisual = bVampire ? Self->VampireVisual : Self->WolfVisual;
            if (!Self || Self->bClosing || Self->Operation!=Expected ||
                !CurrentVisual.Lease.IsValid() || CurrentVisual.Lease.LeaseId!=Lease.LeaseId ||
                CurrentVisual.Lease.ScopeId!=Lease.ScopeId || CurrentVisual.Lease.Generation!=Lease.Generation ||
                CurrentVisual.Lease.IssuerProof!=Lease.IssuerProof) return;
            CurrentVisual.bLoading=false;
            UWorld* CurrentWorld=Self->GetWorld();
            if (!CurrentWorld || CurrentWorld->bIsTearingDown) return;
            USkeletalMesh* Asset=Cast<USkeletalMesh>(CurrentVisual.MeshPath.ResolveObject());
            UClass* Animation=Cast<UClass>(CurrentVisual.AnimPath.ResolveObject());
            const UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(Animation);
            // 狼保留严格Skeleton匹配；吸血鬼仍按既有AnimInstance合同加载。
            const bool bWolfSkeletonMatches = IsValid(Asset) && IsValid(AnimClass) &&
                IsValid(Asset->GetSkeleton()) && AnimClass->GetTargetSkeleton() == Asset->GetSkeleton();
            CurrentVisual.bReady=Result.IsSuccess() && IsValid(Asset) && IsValid(Animation) &&
                Animation->IsChildOf(UAnimInstance::StaticClass()) && (bVampire || bWolfSkeletonMatches);
            if (!CurrentVisual.bReady)
            {
                // 先撤销本类别句柄，再释放自身租约，拒绝回调重入或其它消费者资源被误卸载。
                const FGamePlatformDataLease RejectedLease = CurrentVisual.Lease;
                CurrentVisual.Lease = {};
                if (UGameInstance* Instance = CurrentWorld->GetGameInstance())
                    if (auto* Resources = IGamePlatformDataService::Get(*Instance);
                        Resources && RejectedLease.IsValid())
                        Resources->ReleaseResources(RejectedLease);
                UE_LOG(LogTemp,Warning,TEXT("VillageCreatureVisualRejected: 怪物骨骼或动画蓝图资源未通过校验。"));
                return;
            }
            const auto Wolves=Self->ObservedWolves.Array();
            for (const auto& WeakWolf:Wolves)
            {
                if (Self->bClosing || Self->Operation!=Expected || CurrentWorld->bIsTearingDown) break;
                if (auto* Wolf=WeakWolf.Get(); IsValid(Wolf) &&
                    Wolf->IsA<ADivineBeastsVillageVampire>() == bVampire) Self->ApplyVisual(*Wolf);
            }
        },Accepted);
    if (!Accepted.IsSuccess())
    {
        Visual.bLoading=false;
        UE_LOG(LogTemp,Warning,TEXT("VillageCreatureVisualRequestDenied: GamePlatformData未接受怪物外观租约请求。"));
    }
}

void UDivineBeastsVillageWolfVisualWorldSubsystem::ApplyVisual(ADivineBeastsVillageWolf& Wolf)
{
    const bool bVampire = Wolf.IsA<ADivineBeastsVillageVampire>();
    const FCreatureVisual& Visual = bVampire ? VampireVisual : WolfVisual;
    if (bClosing || !Visual.bReady || !ObservedWolves.Contains(&Wolf) || !IsValid(&Wolf) ||
        Wolf.IsActorBeingDestroyed() || Wolf.GetWorld()!=GetWorld() || !GetWorld() || GetWorld()->bIsTearingDown)
        return;
    auto* Mesh=Wolf.GetMesh();
    auto* Asset=Cast<USkeletalMesh>(Visual.MeshPath.ResolveObject());
    auto* Animation=Cast<UClass>(Visual.AnimPath.ResolveObject());
    UCapsuleComponent* Capsule = Wolf.GetCapsuleComponent();
    const UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(Animation);
    if (!IsValid(Mesh) || !IsValid(Capsule) || !IsValid(Asset) || !IsValid(Animation) ||
        !Animation->IsChildOf(UAnimInstance::StaticClass())) return;
    if (!bVampire && (!IsValid(AnimClass) || !IsValid(Asset->GetSkeleton()) ||
        AnimClass->GetTargetSkeleton() != Asset->GetSkeleton())) return;

    // 狼的新校准键只覆盖狼，保留旧键默认值及吸血鬼自己的比例/朝向配置。
    // 所有值先完成校验，再开始修改Mesh，避免留下半配置外观。
    float EffectiveScale = Visual.Scale;
    float EffectiveYawDegrees = Visual.YawDegrees;
    float HeightOffsetCm = 0.0f;
    if (!bVampire && GConfig)
    {
        constexpr const TCHAR* Section = TEXT("DivineBeasts.VillageWolves");
        float Value = 0.0f;
        if (GConfig->GetFloat(Section, TEXT("WolfVisualScale"), Value, GGameIni))
        {
            if (!FMath::IsFinite(Value) || Value < 0.5f || Value > 2.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualProfileRejected: 新版比例超出审核限制。"));
                return;
            }
            EffectiveScale = Value;
        }
        if (GConfig->GetFloat(Section, TEXT("WolfVisualYawDegrees"), Value, GGameIni))
        {
            if (!FMath::IsFinite(Value) || FMath::Abs(Value) > 180.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualProfileRejected: 新版朝向超出审核限制。"));
                return;
            }
            EffectiveYawDegrees = Value;
        }
        if (GConfig->GetFloat(Section, TEXT("WolfVisualHeightOffsetCm"), Value, GGameIni))
        {
            if (!FMath::IsFinite(Value) || FMath::Abs(Value) > 150.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualProfileRejected: 新版高度偏移超出审核限制。"));
                return;
            }
            HeightOffsetCm = Value;
        }
    }
    // 客户端SkeletalMesh与AnimBP属于已通过平台租约审查的单一内容包；
    // 骨架轴向与模型比例在原生Editor验收后由导入Profile统一修订，绝不改写服务器胶囊。
    Mesh->SetSkeletalMeshAsset(Asset);
    Mesh->bEnableUpdateRateOptimizations = true;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetAnimInstanceClass(Animation);
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetRelativeScale3D(FVector(EffectiveScale));
    const FRotator MeshRotation(0.0f, FMath::UnwindDegrees(EffectiveYawDegrees), 0.0f);
    const FVector GroundedMeshOffset(0.0f, 0.0f, -Capsule->GetScaledCapsuleHalfHeight() + HeightOffsetCm);
    Mesh->SetRelativeRotation(MeshRotation);
    Mesh->SetRelativeLocation(GroundedMeshOffset);
    // Character网络平滑会用缓存的初始Mesh偏移重置模拟代理的位置；
    // 只设置RelativeLocation会使远端狼在下一次网络更新后回到胶囊中心并悬空。
    Wolf.CacheInitialMeshOffset(GroundedMeshOffset, MeshRotation);
    UDivineBeastsDeathDissolveComponent::NotifyVisualsChanged(&Wolf);
    UE_LOG(LogTemp,Log,TEXT("VillageCreatureVisualApplied: 已按世界租约装配怪物模型与动画，Vampire=%d Scale=%.3f"),
        bVampire, EffectiveScale);
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
    WolfVisual.bReady=false; WolfVisual.bLoading=false;
    VampireVisual.bReady=false; VampireVisual.bLoading=false;
    UWorld* World=GetWorld();
    if (World && SpawnHandle.IsValid()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    SpawnHandle.Reset();
    const auto Old=MoveTemp(ObservedWolves);
    ObservedWolves.Reset();
    for (const auto& WeakWolf:Old)
        if (auto* Wolf=WeakWolf.Get()) Wolf->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleWolfEndPlay);
    const FGamePlatformDataLease WolfLease=WolfVisual.Lease;
    const FGamePlatformDataLease VampireLease=VampireVisual.Lease;
    WolfVisual.Lease={}; VampireVisual.Lease={};
    if (World && World->GetGameInstance())
        if (auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance()))
        {
            if (WolfLease.IsValid()) Data->ReleaseResources(WolfLease);
            if (VampireLease.IsValid()) Data->ReleaseResources(VampireLease);
        }
    Super::Deinitialize();
}
