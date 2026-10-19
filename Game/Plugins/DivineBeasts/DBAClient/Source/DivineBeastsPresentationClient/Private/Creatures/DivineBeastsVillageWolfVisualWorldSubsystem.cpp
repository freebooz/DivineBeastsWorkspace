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
<<<<<<< HEAD
    // 未正式导入Fab或未审核时不替换客户端Mesh，不根据网络存在生成假狼外观。
    if (bClosing || bLoading || bReady || bVisualLoadAttempted || !GConfig) return;
    bool bVerified=false;
    const TCHAR* Section=TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section,TEXT("bClientVisualsVerified"),bVerified,GGameIni);
    if (!bVerified)
    {
        // 许可未批准属于正常开发阶段，跳过外观；不产生假的狼资产租约。
=======
    // 未正式导入并核验真实模型时不替换客户端Mesh，不根据网络存在生成假狼外观。
    if (bClosing || bLoading || !GConfig) return;
    bool bVerified=false;
    const TCHAR* Section=TEXT("DivineBeasts.VillageWolves");
    GConfig->GetBool(Section,TEXT("bClientVisualsVerified"),bVerified,GGameIni);
    if (!bVerified) return;
    GConfig->GetFloat(Section,TEXT("WolfMeshScale"),WolfMeshScale,GGameIni);
    GConfig->GetFloat(Section,TEXT("WolfMeshYawDegrees"),WolfMeshYawDegrees,GGameIni);
    if(!FMath::IsFinite(WolfMeshScale)||WolfMeshScale<0.01f||WolfMeshScale>10.0f||
        !FMath::IsFinite(WolfMeshYawDegrees)||FMath::Abs(WolfMeshYawDegrees)>360.0f)
    {
        UE_LOG(LogTemp,Warning,TEXT("VillageWolfVisualProfileRejected: 模型比例或朝向无效。"));
>>>>>>> bbd909b855f26b2bca7627f1188e07469c412a2b
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
<<<<<<< HEAD
                // 已加载但与真实狼骨架不匹配的美术不得持续占用世界资源租约和显存。
                // 失效请求只拒绝本世界这一份租约，不卸载其它系统合法持有的共享资源。
                const FGamePlatformDataLease RejectedLease = Self->VisualLease;
                Self->VisualLease = {};
                if (UGameInstance* Instance = CurrentWorld->GetGameInstance())
                    if (IGamePlatformDataService* Resources = IGamePlatformDataService::Get(*Instance);
                        Resources && RejectedLease.IsValid())
                        Resources->ReleaseResources(RejectedLease);
                UE_LOG(LogTemp, Warning, TEXT("VillageWolfVisualRejected: 狼骨骼或AnimBlueprint与已审核资源不匹配，已释放本次资源租约。"));
=======
                UE_LOG(LogTemp,Warning,TEXT("VillageWolfVisualRejected: 狼的骨骼或动画蓝图资源未通过真实导入/校验。"));
>>>>>>> bbd909b855f26b2bca7627f1188e07469c412a2b
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
    const UAnimBlueprintGeneratedClass* AnimClass=Cast<UAnimBlueprintGeneratedClass>(Animation);
    if (!IsValid(Mesh) || !IsValid(Asset) || !IsValid(AnimClass) ||
        !IsValid(Asset->GetSkeleton()) || AnimClass->GetTargetSkeleton()!=Asset->GetSkeleton()) return;
    // 客户端SkeletalMesh与AnimBP属于已通过平台租约审查的单一内容包；
    // 骨架轴向与模型比例在原生Editor验收后由导入Profile统一修订，绝不改写服务器胶囊。
    Mesh->SetSkeletalMeshAsset(Asset);
    // 狼群最多20只：仅客户端开启骨骼动画更新率优化；不可见时停止刷新完整骨骼姿态。
    // 服务器碰撞、寻路、命中和Actor复制不依赖这份纯表现更新频率。
    Mesh->bEnableUpdateRateOptimizations = true;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
    Mesh->SetAnimInstanceClass(Animation);
    if (bClosing || Wolf.IsActorBeingDestroyed()) return;
<<<<<<< HEAD
    // 狼与人体模型轴向、比例可能不同；只有美术审核后显式填写的项目参数才覆盖默认变换。
    // 这些值只作用于客户端视觉骨骼，不能改变服务器胶囊、战斗判定或物理。
    float HeightOffsetCm = 0.0f;
    if (GConfig)
    {
        constexpr const TCHAR* Section = TEXT("DivineBeasts.VillageWolves");
        float YawDegrees = 0.0f;
        if (GConfig->GetFloat(Section, TEXT("WolfVisualYawDegrees"), YawDegrees, GGameIni) &&
            FMath::IsFinite(YawDegrees) && FMath::Abs(YawDegrees) <= 180.0f)
            Mesh->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));

        float VisualScale = 1.0f;
        if (GConfig->GetFloat(Section, TEXT("WolfVisualScale"), VisualScale, GGameIni) &&
            FMath::IsFinite(VisualScale) && VisualScale >= 0.5f && VisualScale <= 2.0f)
            Mesh->SetRelativeScale3D(FVector(VisualScale));

        float ConfiguredHeight = 0.0f;
        if (GConfig->GetFloat(Section, TEXT("WolfVisualHeightOffsetCm"), ConfiguredHeight, GGameIni) &&
            FMath::IsFinite(ConfiguredHeight) && FMath::Abs(ConfiguredHeight) <= 150.0f)
            HeightOffsetCm = ConfiguredHeight;
    }
    Mesh->SetRelativeLocation(FVector(0.0f, 0.0f,
        -Wolf.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + HeightOffsetCm));
=======
    Mesh->SetRelativeScale3D(FVector(WolfMeshScale));
    const FRotator MeshRotation(0.0f, WolfMeshYawDegrees, 0.0f);
    const FVector GroundedMeshOffset(0.0f, 0.0f, -Wolf.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Mesh->SetRelativeRotation(MeshRotation);
    Mesh->SetRelativeLocation(GroundedMeshOffset);
    // Character网络平滑会用缓存的初始Mesh偏移重置模拟代理的位置；
    // 只设置RelativeLocation会使远端狼在下一次网络更新后回到胶囊中心并悬空。
    Wolf.CacheInitialMeshOffset(GroundedMeshOffset, MeshRotation);
    UE_LOG(LogTemp,Log,TEXT("VillageWolfVisualApplied: 已按世界租约装配真实狼模型与动画，Scale=%.3f"),WolfMeshScale);
>>>>>>> bbd909b855f26b2bca7627f1188e07469c412a2b
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
