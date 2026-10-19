// SSLM狼群外观由客户端从已授权内容包装配；权威AI/Combat不依赖第三方模型的作者或资源格式。
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
#include "Animation/AnimBlueprintGeneratedClass.h" // 校验真实动画蓝图生成类与骨骼网格的Skeleton一致。

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
    bVisualLoadAttempted = false;
    bMissingVisualLogged = false;
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
    // 未经资源许可/原生审核绝不替换客户端网格或创建假的狼外观。
    // 同一World最多发出一次真实资源租约，避免多只狼反复触发加载。
    if (bClosing || bLoading || bReady || bVisualLoadAttempted || !GConfig) return;
    bool bVerified = false;
    const TCHAR* Section = TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section, TEXT("bClientVisualsVerified"), bVerified, GGameIni);
    if (!bVerified) return;

    // 兼容原有已审核校准键；默认比例/朝向均不覆盖角色原始坐标系。
    GConfig->GetFloat(Section, TEXT("WolfMeshScale"), WolfMeshScale, GGameIni);
    GConfig->GetFloat(Section, TEXT("WolfMeshYawDegrees"), WolfMeshYawDegrees, GGameIni);
    if (!FMath::IsFinite(WolfMeshScale) || WolfMeshScale < 0.01f || WolfMeshScale > 10.0f ||
        !FMath::IsFinite(WolfMeshYawDegrees) || FMath::Abs(WolfMeshYawDegrees) > 360.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualProfileRejected: 已审核狼模型的比例或朝向参数无效。"));
        return;
    }
    FString MeshName, AnimName;
    GConfig->GetString(Section,TEXT("WolfSkeletalMesh"),MeshName,GGameIni);
    GConfig->GetString(Section,TEXT("WolfAnimationBlueprint"),AnimName,GGameIni);
    WolfMeshPath=FSoftObjectPath(MeshName);
    WolfAnimPath=FSoftObjectPath(AnimName);
    // 客户端也执行与专服相同的第三方内容包根路径保护；未注册的其它狼材质不允许旁路加载。
    const FString AnimalContentRoot(TEXT("/DBAContentPack_AnimalsProtofactor/"));
    if (!WolfMeshPath.IsValid() || !WolfAnimPath.IsValid() ||
        !WolfMeshPath.GetLongPackageName().StartsWith(AnimalContentRoot) ||
        !WolfAnimPath.GetLongPackageName().StartsWith(AnimalContentRoot))
    {
        if (!bMissingVisualLogged)
            UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualConfigMissing: 真实Fab狼骨骼/AnimBlueprint不存在或未归集通用动物包。"));
        bMissingVisualLogged = true;
        return;
    }
    UWorld* World=GetWorld();
    if (!World || World->bIsTearingDown || !World->GetGameInstance()) return;
    auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance());
    if (!Data) return;
    bVisualLoadAttempted = true;
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
            const UAnimBlueprintGeneratedClass* AnimClass=Cast<UAnimBlueprintGeneratedClass>(Animation);
            // 不能只凭继承UAnimInstance就接受不匹配的动画蓝图；多人狼群必须严格匹配同一骨架。
            Self->bReady=Result.IsSuccess() && IsValid(Asset) && IsValid(AnimClass) &&
                IsValid(Asset->GetSkeleton()) && AnimClass->GetTargetSkeleton()==Asset->GetSkeleton();
            if (!Self->bReady)
            {
                // 已加载但不匹配实际Skeleton/AnimBlueprint的外观不得继续持有世界资源租约。
                // 只释放本次拥有的Data租约，不卸载其它消费者合法共享的资源。
                const FGamePlatformDataLease RejectedLease = Self->VisualLease;
                Self->VisualLease = {};
                if (UGameInstance* Instance = CurrentWorld->GetGameInstance())
                    if (IGamePlatformDataService* Resources = IGamePlatformDataService::Get(*Instance);
                        Resources && RejectedLease.IsValid())
                        Resources->ReleaseResources(RejectedLease);
                UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualRejected: 狼Mesh/Skeleton/AnimBlueprint未通过匹配检查，本世界租约已释放。"));
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
        Wolf.IsActorBeingDestroyed() || Wolf.GetWorld() != GetWorld() ||
        !GetWorld() || GetWorld()->bIsTearingDown)
        return;

    USkeletalMeshComponent* Mesh = Wolf.GetMesh();
    UCapsuleComponent* Capsule = Wolf.GetCapsuleComponent();
    USkeletalMesh* Asset = Cast<USkeletalMesh>(WolfMeshPath.ResolveObject());
    UClass* Animation = Cast<UClass>(WolfAnimPath.ResolveObject());
    const UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(Animation);
    if (!IsValid(Mesh) || !IsValid(Capsule) || !IsValid(Asset) || !IsValid(AnimClass) ||
        !IsValid(Asset->GetSkeleton()) || AnimClass->GetTargetSkeleton() != Asset->GetSkeleton())
        return;

    // 已审核旧版WolfMeshScale/Yaw仍有效；新版WolfVisual*仅在用户显式配置且取值安全时覆盖。
    // 校验必须发生在任何Mesh变更之前，避免配置错误时留下半配置客户端模型。
    float EffectiveScale = WolfMeshScale;
    float EffectiveYawDegrees = WolfMeshYawDegrees;
    float HeightOffsetCm = 0.0f;
    if (GConfig)
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

    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    const FRotator MeshRotation(0.0f, FMath::UnwindDegrees(EffectiveYawDegrees), 0.0f);
    const FVector GroundedMeshOffset(0.0f, 0.0f,
        -Capsule->GetScaledCapsuleHalfHeight() + HeightOffsetCm);

    // 所有变化都只作用于复制角色的客户端骨骼网格，不改变服务器碰撞/导航/伤害。
    Mesh->SetSkeletalMeshAsset(Asset);
    Mesh->bEnableUpdateRateOptimizations = true;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetAnimInstanceClass(Animation);
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetRelativeScale3D(FVector(EffectiveScale));
    Mesh->SetRelativeRotation(MeshRotation);
    Mesh->SetRelativeLocation(GroundedMeshOffset);
    // Character网络平滑会按缓存重置模拟代理网格位姿；不能只改RelativeLocation。
    Wolf.CacheInitialMeshOffset(GroundedMeshOffset, MeshRotation);
    UE_LOG(LogTemp, Log, TEXT("VillageWolfVisualApplied: 已装配真实狼骨骼/动画，Scale=%.3f"), EffectiveScale);
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
    bVisualLoadAttempted = true;
    bMissingVisualLogged = false;
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
