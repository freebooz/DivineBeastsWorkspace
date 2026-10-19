// 项目纯视觉：复制施法事实→只读目录/动画；统一Data租约，不使用Tick、HTTP或Niagara执行器。
#include "Development/DivineBeastsDevelopmentCastingWorldSubsystem.h"
#include "Development/DivineBeastsPlaceholderMagicVisualPolicy.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"

namespace
{
/** 精确允许列表，网络事实不能拼接任意外部路径。 */
FName CastName(FName Style)
{
    if(Style==TEXT("DBA.Development.Fireball")) return TEXT("Fireball");
    if(Style==TEXT("DBA.Development.DirectedMissile")) return TEXT("DirectedMissile");
    if(Style==TEXT("DBA.Development.Frostbolt")) return TEXT("Frostbolt");
    if(Style==TEXT("DBA.Development.ArcaneMissiles")) return TEXT("ArcaneMissiles");
    return NAME_None;
}
FSoftObjectPath MontagePath(FName Name)
{
    if (Name==TEXT("DirectedMissile")) Name=TEXT("Fireball"); // 内容复用，同一预载租约，无新增动画播放器。
    const FString Asset=TEXT("AM_DBA_Dev")+Name.ToString()+TEXT("Cast");
    return FSoftObjectPath(TEXT("/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/")+Asset+TEXT(".")+Asset);
}
/** 同一施法事实按当前Mesh骨架选用已导入的人型施法动作，不信任网络携带资产路径。 */
FSoftObjectPath MontagePathForMesh(FName Name, const USkeletalMesh* Mesh)
{
    const USkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
    const FString SkeletonPath = IsValid(Skeleton) ? Skeleton->GetPathName() : FString();
    if (SkeletonPath == TEXT("/DBAContentPack_Common/Characters/KachujinGRosales/Kachujin_G_Rosales_Skeleton.Kachujin_G_Rosales_Skeleton"))
        return FSoftObjectPath(TEXT("/DBAContentPack_Common/Characters/KachujinGRosales/Animations/AM_DBA_Kachujin_Cast.AM_DBA_Kachujin_Cast"));
    if (SkeletonPath == TEXT("/DBAContentPack_Common/Characters/NPC/Arissa/Arissa_Skeleton.Arissa_Skeleton"))
        return FSoftObjectPath(TEXT("/DBAContentPack_Common/Characters/NPC/Arissa/Animations/AM_DBA_Arissa_Cast.AM_DBA_Arissa_Cast"));
    if (SkeletonPath == TEXT("/DBAWorldPack_Village/Creatures/VampireALusth/Vampire_A_Lusth_Skeleton.Vampire_A_Lusth_Skeleton"))
        return FSoftObjectPath(TEXT("/DBAWorldPack_Village/Creatures/VampireALusth/Animations/AM_DBA_Vampire_Cast.AM_DBA_Vampire_Cast"));
    return MontagePath(Name);
}
FGameplayTag CastSemantic(FName Name)
{ return FGameplayTag::RequestGameplayTag(FName(*(TEXT("Presentation.DivineBeasts.Development.Magic.")+Name.ToString()+TEXT(".Cast"))),false); }
/** 组件广播传值后允许同步发布后继；必须比较完整当前快照，而非仅凭旧通知的Active标记清理。 */
bool SameCastFacts(const FGamePlatformAbilityCastFacts& A,const FGamePlatformAbilityCastFacts& B)
{
    return A.ActivationId==B.ActivationId && A.AbilityId==B.AbilityId &&
        A.PresentationStyleId==B.PresentationStyleId && A.AvatarGeneration==B.AvatarGeneration &&
        A.StartedServerSeconds==B.StartedServerSeconds && A.DurationSeconds==B.DurationSeconds && A.bActive==B.bActive;
}
}

bool UDivineBeastsDevelopmentCastingWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World=Cast<UWorld>(Outer);
    // 正式Village仅允许已授予的占位技能播放起手表现；实际资格再由GetCastWindow按事实严格校验。
    return !IsRunningCommandlet() && World && World->IsGameWorld() && World->GetNetMode()!=NM_DedicatedServer &&
        (UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess() ||
         DivineBeastsPlaceholderMagicVisualPolicy::IsVillageWorld(World));
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World); LifetimeToken=FGuid::NewGuid();
    if(!World.GetGameInstance()) return;
    // 不固定第零玩家；世界只有一份动画执行，选择属于本世界的实际本地表现作用域。
    for(auto* Player:World.GetGameInstance()->GetLocalPlayers())
        if(Player && Player->GetWorld()==&World)
            if(auto* Adapter=Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>())
            { Presentation=Adapter; PlatformPresentation=Player->GetSubsystem<UGamePlatformPresentationClientSubsystem>(); break; }
    if(Presentation.IsValid()) PackHandle=Presentation->OnContentPackChanged().AddUObject(this,&ThisClass::HandlePack);
    SpawnHandle=World.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&ThisClass::ObserveActor));
    for(TActorIterator<ACharacter> It(&World);It;++It) ObserveActor(*It);
    auto* Data=IGamePlatformDataService::Get(*World.GetGameInstance());
    if(!Data) { UE_LOG(LogTemp,Warning,TEXT("开发施法动画缺少Data服务；权威技能不受影响")); return; }
    TArray<FSoftObjectPath> Paths;
    for(FName Name:{FName(TEXT("Fireball")),FName(TEXT("Frostbolt")),FName(TEXT("ArcaneMissiles"))}) Paths.Add(MontagePath(Name));
    Paths.Add(FSoftObjectPath(TEXT("/DBAContentPack_Common/Characters/KachujinGRosales/Animations/AM_DBA_Kachujin_Cast.AM_DBA_Kachujin_Cast")));
    Paths.Add(FSoftObjectPath(TEXT("/DBAContentPack_Common/Characters/NPC/Arissa/Animations/AM_DBA_Arissa_Cast.AM_DBA_Arissa_Cast")));
    Paths.Add(FSoftObjectPath(TEXT("/DBAWorldPack_Village/Creatures/VampireALusth/Animations/AM_DBA_Vampire_Cast.AM_DBA_Vampire_Cast")));
    FGamePlatformResult Result; const FGuid Token=LifetimeToken;
    TWeakObjectPtr<UDivineBeastsDevelopmentCastingWorldSubsystem> WeakThis(this);
    VisualLease=Data->AcquireResources(Paths,EGamePlatformDataLifetime::World,this,
        [WeakThis,Token](const FGamePlatformDataLease& Lease,const FGamePlatformResult& Completed)
        {
            auto* Self=WeakThis.Get();
            if(!Self || Self->bClosing || !Self->GetWorld() || Self->GetWorld()->bIsTearingDown ||
                Self->LifetimeToken!=Token || Self->VisualLease.LeaseId!=Lease.LeaseId) return;
            Self->bReady=Completed.IsSuccess();
            if(!Self->bReady) { UE_LOG(LogTemp,Warning,TEXT("开发施法动画预热失败：%s"),*Completed.Code.ToString()); return; }
            Self->RefreshFacts();
        },Result);
}

void UDivineBeastsDevelopmentCastingWorldSubsystem::ObserveActor(AActor* Actor)
{
    auto* Character=Cast<ACharacter>(Actor);
    auto* Component=Character ? Character->FindComponentByClass<UGamePlatformAbilityCastComponent>() : nullptr;
    if(bClosing || !Component || Actor->GetWorld()!=GetWorld() || Bindings.Contains(Component)) return;
    FBinding Binding; Binding.FactsHandle=Component->OnCastFactsChanged().AddUObject(this,&ThisClass::HandleFacts);
    Binding.Mesh=Character->GetMesh();
    if(auto* Mesh=Binding.Mesh.Get()) Mesh->OnAnimInitialized.AddUniqueDynamic(this,&ThisClass::HandleAnimInitialized);
    Binding.Identity=Character->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if(auto* Identity=Binding.Identity.Get())
    {
        const FGuid Token=LifetimeToken;
        TWeakObjectPtr<UGamePlatformAbilityCastComponent> WeakComponent(Component);
        // 弱Owner仅防止销毁后调用；生命周期身份同时防止旧世界的通知触及下一作用域。
        Binding.IdentityHandle=Identity->OnIdentityChanged().AddWeakLambda(this,[this,WeakComponent,Token]()
        {
            if(!bClosing && LifetimeToken==Token)
                if(auto* Current=WeakComponent.Get()) HandleFacts(Current,Current->GetCastFacts());
        });
        Binding.ReadinessHandle=Identity->OnReadinessChanged().AddWeakLambda(this,[this,WeakComponent,Token](bool)
        {
            if(!bClosing && LifetimeToken==Token)
                if(auto* Current=WeakComponent.Get()) HandleFacts(Current,Current->GetCastFacts());
        });
    }
    Bindings.Add(Component,Binding); Actor->OnEndPlay.AddUniqueDynamic(this,&ThisClass::HandleActorEndPlay);
    HandleFacts(Component,Component->GetCastFacts());
}
bool UDivineBeastsDevelopmentCastingWorldSubsystem::GetCastWindow(UGamePlatformAbilityCastComponent* Component,
    const FGamePlatformAbilityCastFacts& Facts,double& ElapsedSeconds,double& RemainingSeconds) const
{
    const auto* World=GetWorld();
    if(bClosing || !World || World->bIsTearingDown || !IsValid(Component) || Component->GetWorld()!=World ||
        !Bindings.Contains(Component) || !SameCastFacts(Facts,Component->GetCastFacts()) || !Facts.bActive ||
        !Facts.ActivationId.IsValid() || Facts.AbilityId.IsNone() || CastName(Facts.PresentationStyleId).IsNone() ||
        Facts.AvatarGeneration<=0 || !FMath::IsFinite(Facts.StartedServerSeconds) ||
        !FMath::IsFinite(Facts.DurationSeconds) || Facts.DurationSeconds<=0 || Facts.DurationSeconds>10) return false;
    // 扩展到Village不意味着放开开发魔法权限：仅接受十二生肖自己的合法占位技能。
    if (!UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess() &&
        (!DivineBeastsPlaceholderMagicVisualPolicy::IsVillageWorld(World) ||
         !DivineBeastsPlaceholderMagicVisualPolicy::IsApprovedPlaceholderSpell(
             Facts.AbilityId, CastName(Facts.PresentationStyleId)))) return false;
    const auto* Character=Cast<ACharacter>(Component->GetOwner());
    const auto* Combat=Character ? Character->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    const auto* Identity=Character ? Character->FindComponentByClass<UDivineBeastsCharacterComponent>() : nullptr;
    if(!IsValid(Character) || Character->IsActorBeingDestroyed() || !Combat) return false;
    // 项目原子身份是客户端收到同代权威Avatar的只读回执；不等待另一个组件字段的复制先后。
    // Combat仍是唯一战斗权威；旧代死亡副本不能把已收到的新代施法误判为死亡。
    const int32 Generation=Identity ? Identity->GetAvatarGeneration() : Combat->GetCombatAvatarGeneration();
    if(Generation!=Facts.AvatarGeneration ||
        (Combat->GetCombatAvatarGeneration()==Generation && Combat->IsCombatDead())) return false;
    const auto* State=World->GetGameState();
    const double Now=State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    if(!FMath::IsFinite(Now)) return false;
    ElapsedSeconds=FMath::Max(0.0,Now-Facts.StartedServerSeconds);
    RemainingSeconds=Facts.DurationSeconds-ElapsedSeconds;
    return RemainingSeconds>0;
}
bool UDivineBeastsDevelopmentCastingWorldSubsystem::IsCastOperationCurrent(UGamePlatformAbilityCastComponent* Component,
    const FGamePlatformAbilityCastFacts& Facts,const FGuid& OperationId,const FGuid& Token,UAnimInstance* Anim) const
{
    double ElapsedSeconds=0, RemainingSeconds=0;
    if(LifetimeToken!=Token || !IsValid(Anim) || !GetCastWindow(Component,Facts,ElapsedSeconds,RemainingSeconds)) return false;
    const auto* Binding=Bindings.Find(Component);
    const auto* Character=Cast<ACharacter>(Component->GetOwner());
    const auto* Mesh=Character ? Character->GetMesh() : nullptr;
    return Binding && Binding->ActivationId==Facts.ActivationId && Binding->OperationId==OperationId &&
        Binding->Anim.Get()==Anim && Mesh && Binding->Mesh.Get()==Mesh && Mesh->GetAnimInstance()==Anim &&
        Binding->SkeletalMesh.Get()==Mesh->GetSkeletalMeshAsset();
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::HandleFacts(UGamePlatformAbilityCastComponent* Component,const FGamePlatformAbilityCastFacts& Incoming)
{
    const auto Facts=Incoming;
    if(bClosing || !IsValid(Component) || Component->GetWorld()!=GetWorld() || !Bindings.Contains(Component)) return;
    // 外层A取消广播返回前，其他监听者可以同步发布B；旧A快照不能操作此刻已属于B的账本。
    if(!SameCastFacts(Facts,Component->GetCastFacts())) return;
    double Elapsed=0, Remaining=0;
    if(!GetCastWindow(Component,Facts,Elapsed,Remaining)) { StopCastAudio(Component); StopCast(Component); return; }
    TWeakObjectPtr<UGamePlatformAbilityCastComponent> WeakComponent(Component);
    // 起手声音属于真实施法窗口；缺少动画、Niagara或外观重建均不能推迟到弹体发射才响。
    UpdateCastAudio(Component,Facts,Remaining);
    Component=WeakComponent.Get();
    if(!GetCastWindow(Component,Facts,Elapsed,Remaining)) return; // 声音提交可能同步取消或换代。
    TWeakObjectPtr<ACharacter> WeakCharacter(Cast<ACharacter>(Component->GetOwner()));
    auto* Character=WeakCharacter.Get();
    auto* Mesh=Character->GetMesh();
    auto* Anim=Mesh ? Mesh->GetAnimInstance() : nullptr;
    const FName Name=CastName(Facts.PresentationStyleId);
    auto* Montage=Cast<UAnimMontage>(MontagePathForMesh(Name, Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr).ResolveObject());
    auto* Binding=Bindings.Find(Component);
    if(Binding->ActivationId.IsValid() && (Binding->ActivationId!=Facts.ActivationId || Binding->Anim.Get()!=Anim ||
        !Mesh || Binding->SkeletalMesh.Get()!=Mesh->GetSkeletalMeshAsset() || Binding->Montage.Get()!=Montage))
    {
        // 后继或外观换代先清自有旧资源；Stop包含同步通知，之后不再借用旧Binding/Character地址。
        StopCast(Component);
        Component=WeakComponent.Get(); Character=WeakCharacter.Get();
        if(!GetCastWindow(Component,Facts,Elapsed,Remaining) || !IsValid(Character)) return;
        Mesh=Character->GetMesh(); Anim=Mesh ? Mesh->GetAnimInstance() : nullptr;
        Montage=Cast<UAnimMontage>(MontagePathForMesh(Name, Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr).ResolveObject()); Binding=Bindings.Find(Component);
    }
    if(!bReady || !Binding || !Montage || !IsValid(Anim) || !Mesh || !Mesh->GetSkeletalMeshAsset() ||
        Montage->GetSkeleton()!=Mesh->GetSkeletalMeshAsset()->GetSkeleton()) return;
    TWeakObjectPtr<UAnimInstance> WeakAnim(Anim);
    const FGuid Token=LifetimeToken;
    if(Binding->ActivationId!=Facts.ActivationId)
    {
        Binding->ActivationId=Facts.ActivationId; Binding->OperationId=FGuid::NewGuid();
        Binding->Anim=Anim; Binding->Montage=Montage; Binding->SkeletalMesh=Mesh->GetSkeletalMeshAsset(); Binding->StyleName=Name;
        Binding->bStartingMontage=true;
        const FGuid StartedOperation=Binding->OperationId;
        // 先建立可撤销Timer所有权；Montage_Play会同步广播Started，取消时必须已能完整摘除本窗口。
        GetWorld()->GetTimerManager().SetTimer(Binding->StopTimer,FTimerDelegate::CreateWeakLambda(this,[this,WeakComponent,StartedOperation,Token]()
        {
            if(bClosing || LifetimeToken!=Token || !WeakComponent.IsValid()) return;
            if(const auto* Current=Bindings.Find(WeakComponent); Current && Current->OperationId==StartedOperation)
                StopCast(WeakComponent.Get(),StartedOperation);
        }),Remaining,false);
        // UE先登记实例再同步广播Started；监听者可重播同一资产，不能用返回时的Active实例推断所有权。
        // 只保存公开实例ID，不跨Play保留实例指针；多个新增匹配实例时保守不认领任何一个。
        TSet<int32> PreviousMontageInstanceIds;
        for(const auto* Instance:Anim->MontageInstances)
            if(Instance) PreviousMontageInstanceIds.Add(Instance->GetInstanceID());
        TWeakObjectPtr<UAnimMontage> WeakMontage(Montage);
        // 保持false作用域，不修改Skeleton/Group；有效起始偏移只播放当前剩余阶段。
        // 冰箭动作映射至服务器复制的完整施法窗口；迟到事件也按同一比例进入剩余动作，避免2秒条配0.7秒动作。
        const bool bImportedHumanoidCast = MontagePathForMesh(Name, Mesh->GetSkeletalMeshAsset()) != MontagePath(Name);
        const float PlayRate=(bImportedHumanoidCast || Name==TEXT("Frostbolt"))
            ? Montage->GetPlayLength()/Facts.DurationSeconds : 1.0f;
        const float PlayedLength=Anim->Montage_Play(Montage,PlayRate,EMontagePlayReturnType::MontageLength,static_cast<float>(Elapsed)*PlayRate,false);
        // Started可能取消、换Avatar、销毁Actor或扩容Bindings；绝不跨此外调保存TMap元素指针。
        Component=WeakComponent.Get(); Anim=WeakAnim.Get();
        int32 StartedMontageInstanceId=INDEX_NONE;
        int32 NewMatchingInstanceCount=0;
        if(Anim && WeakMontage.IsValid())
            for(const auto* Instance:Anim->MontageInstances)
                if(Instance && Instance->Montage==WeakMontage.Get() && !PreviousMontageInstanceIds.Contains(Instance->GetInstanceID()))
                { ++NewMatchingInstanceCount; StartedMontageInstanceId=Instance->GetInstanceID(); }
        if(NewMatchingInstanceCount!=1) StartedMontageInstanceId=INDEX_NONE;
        if(!IsCastOperationCurrent(Component,Facts,StartedOperation,Token,Anim) || PlayedLength<=0)
        {
            // 启动中取消已先撤Timer/VFX/Anchor，动画必须等此Play返回才能确定身份并在同一GT帧清理。
            // ExpectedOperation保护同步启动的后继B；局部退休快照也只停止唯一确认且仍活动的A实例。
            StopCast(Component,StartedOperation);
            FBinding RetiredMontage; RetiredMontage.Anim=WeakAnim; RetiredMontage.Montage=WeakMontage;
            RetiredMontage.MontageInstanceId=StartedMontageInstanceId;
            ReleaseCastResources(RetiredMontage);
            return;
        }
        Binding=Bindings.Find(Component); Binding->bStartingMontage=false;
        Binding->MontageInstanceId=StartedMontageInstanceId;
    }
    else if(Binding->bStartingMontage) return;

    // 动画去重与VFX受理分离：先前ProviderMissing允许当前窗口重试，已受理/正在提交则不能重复。
    const FGuid Operation=Binding->OperationId;
    if(Binding->VFXRequestId.IsValid() || Binding->bSubmittingVFX) return;
    if(!IsCastOperationCurrent(WeakComponent.Get(),Facts,Operation,Token,WeakAnim.Get()))
    { StopCast(WeakComponent.Get(),Operation); return; }
    TWeakObjectPtr<USceneComponent> WeakAnchor=Binding->CastAnchor;
    if(!WeakAnchor.IsValid())
    {
        Character=WeakCharacter.Get(); Mesh=Character->GetMesh();
        auto* Anchor=NewObject<USceneComponent>(Character);
        Anchor->SetMobility(EComponentMobility::Movable);
        if (Name == TEXT("Frostbolt"))
        {
            // 冰箭地面符文与冰晶升腾采用脚底为局部原点；右手挂点会让法阵悬空。
            auto* Capsule = Character->GetCapsuleComponent();
            if (!Capsule) return;
            Anchor->SetupAttachment(Capsule);
            Anchor->SetRelativeLocation(FVector(0,0,-Capsule->GetScaledCapsuleHalfHeight()));
        }
        else Anchor->SetupAttachment(Mesh,TEXT("hand_r"));
        Character->AddInstanceComponent(Anchor);
        Binding->CastAnchor=Anchor; WeakAnchor=Anchor; // Register之前先交付所有权，重入结束能清自己的锚点。
        Anchor->RegisterComponent();
        if(!IsCastOperationCurrent(WeakComponent.Get(),Facts,Operation,Token,WeakAnim.Get()))
        { StopCast(WeakComponent.Get(),Operation); return; }
        Binding=Bindings.Find(WeakComponent);
    }
    // RegisterComponent同样可能同步通知：若嵌套栈已受理起手，外层不得再提交第二个Request。
    if(Binding->VFXRequestId.IsValid() || Binding->bSubmittingVFX) return;
    auto* Anchor=WeakAnchor.Get(); auto* Adapter=PlatformPresentation.Get();
    if(!Anchor || !Anchor->IsRegistered() || !Adapter) return;
    FGamePlatformPresentationRequest Request; Request.RequestId=FGuid::NewGuid(); Request.SemanticTag=CastSemantic(Name);
    Request.SourceId=Facts.AbilityId; Request.ContextId=Facts.PresentationStyleId;
    Request.SourceLocation=Anchor->GetComponentLocation(); Request.AttachComponent=Anchor;
    Request.Context.ProjectId=FDivineBeastsProjectCatalog::GetProjectId(); Request.Context.AvatarGeneration=Facts.AvatarGeneration;
    Request.RequestGeneration=Facts.AvatarGeneration; Request.WorldGeneration=Adapter->GetWorldGeneration();
    Request.Lifetime=EGamePlatformPresentationLifetime::Persistent;
    TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> WeakAdapter(Adapter);
    Binding->VFXRequestId=Request.RequestId; Binding->VFXAdapter=Adapter;
    Binding->VFXWorldGeneration=Request.WorldGeneration; Binding->bSubmittingVFX=true;
    const auto Submitted=Adapter->Submit(Request);
    if(!IsCastOperationCurrent(WeakComponent.Get(),Facts,Operation,Token,WeakAnim.Get()))
    {
        StopCast(WeakComponent.Get(),Operation);
        // 提交中取消由本提交栈负责终态，避免提供者尚未受理时提前取消，返回后又遗留实例。
        if(Submitted==EGamePlatformPresentationSubmitResult::Submitted)
            if(auto* CurrentAdapter=WeakAdapter.Get())
            { Request.PredictionState=EGamePlatformPresentationPredictionState::Cancelled; CurrentAdapter->Submit(Request); }
        return;
    }
    Binding=Bindings.Find(WeakComponent); Binding->bSubmittingVFX=false;
    if(Submitted!=EGamePlatformPresentationSubmitResult::Submitted)
    { Binding->VFXRequestId.Invalidate(); Binding->VFXAdapter.Reset(); Binding->VFXWorldGeneration=0; }
}

void UDivineBeastsDevelopmentCastingWorldSubsystem::UpdateCastAudio(
    UGamePlatformAbilityCastComponent* Component,const FGamePlatformAbilityCastFacts& Facts,double RemainingSeconds)
{
    auto* Binding=Bindings.Find(Component); auto* Adapter=PlatformPresentation.Get();
    if(!Binding || !Adapter) return;
    const FGuid Token=LifetimeToken;
    const TWeakObjectPtr<UGamePlatformAbilityCastComponent> WeakComponent(Component);
    const TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> WeakAdapter(Adapter);
    if(Binding->AudioActivationId!=Facts.ActivationId)
    {
        StopCastAudio(Component);
        double Elapsed=0, Remaining=0;
        Component=WeakComponent.Get();
        if(LifetimeToken!=Token || !GetCastWindow(Component,Facts,Elapsed,Remaining)) return;
        RemainingSeconds=Remaining; Binding=Bindings.Find(Component);
        Binding->AudioActivationId=Facts.ActivationId; Binding->bAudioAccepted=false;
    }
    if(Binding->AudioRequestId.IsValid() || Binding->bSubmittingAudio || Binding->bAudioAccepted) return;
    const FName Name=CastName(Facts.PresentationStyleId);
    const auto Semantic=FGameplayTag::RequestGameplayTag(Name==TEXT("Frostbolt")
        ? FName(TEXT("Presentation.SSLM.Audio.Spell.Frost.Cast"))
        : FName(TEXT("Presentation.SSLM.AudioLibrary.Spell.ArcaneCast")),false);
    if(Name.IsNone() || !Semantic.IsValid()) return;
    FGamePlatformPresentationRequest Request;
    Request.RequestId=FGuid::NewGuid(); Request.SemanticTag=Semantic;
    Request.ContextId=Facts.PresentationStyleId; Request.SourceId=Facts.AbilityId;
    Request.SourceLocation=Component->GetOwner()->GetActorLocation();
    Request.Context.ProjectId=FDivineBeastsProjectCatalog::GetProjectId();
    Request.Context.AvatarGeneration=Facts.AvatarGeneration; Request.RequestGeneration=Facts.AvatarGeneration;
    Adapter=WeakAdapter.Get(); if(!Adapter) return;
    Request.WorldGeneration=Adapter->GetWorldGeneration();
    double CheckedElapsed=0, CheckedRemaining=0;
    if(LifetimeToken!=Token || !GetCastWindow(WeakComponent.Get(),Facts,CheckedElapsed,CheckedRemaining)) return;
    Binding=Bindings.Find(WeakComponent); Adapter=WeakAdapter.Get();
    if(!Adapter || !Binding || Binding->AudioRequestId.IsValid() || Binding->bAudioAccepted) return;
    RemainingSeconds=CheckedRemaining;
    Request.Lifetime=EGamePlatformPresentationLifetime::Persistent;
    const FGuid Activation=Facts.ActivationId;
    Binding->AudioRequestId=Request.RequestId; Binding->AudioSemantic=Semantic;
    Binding->AudioAdapter=Adapter; Binding->AudioWorldGeneration=Request.WorldGeneration; Binding->bSubmittingAudio=true;
    // 只安排此表现的结束，不轮询业务，也不提前结束服务器施法。迟到快照只保留真实剩余窗口。
    GetWorld()->GetTimerManager().SetTimer(Binding->AudioStopTimer,
        FTimerDelegate::CreateWeakLambda(this,[this,WeakComponent,Activation,Token]()
        { if(!bClosing && LifetimeToken==Token) StopCastAudio(WeakComponent.Get(),Activation); }),
        static_cast<float>(RemainingSeconds),false);
    const auto Result=Adapter->Submit(Request);
    double Elapsed=0, Remaining=0;
    Binding=Bindings.Find(WeakComponent);
    if(LifetimeToken!=Token || !Binding || Binding->AudioRequestId!=Request.RequestId ||
        !GetCastWindow(WeakComponent.Get(),Facts,Elapsed,Remaining))
    {
        StopCastAudio(WeakComponent.Get(),Activation);
        if(Result==EGamePlatformPresentationSubmitResult::Submitted)
            if(auto* Current=WeakAdapter.Get())
            { Request.PredictionState=EGamePlatformPresentationPredictionState::Cancelled; Current->Submit(Request); }
        return;
    }
    Binding->bSubmittingAudio=false;
    if(Result!=EGamePlatformPresentationSubmitResult::Submitted) StopCastAudio(WeakComponent.Get(),Activation);
    else
    {
        Binding->bAudioAccepted=true;
        UE_LOG(LogTemp,Log,TEXT("SSLM_CAST_AUDIO Stage=Cast Style=%s Activation=%s Remaining=%.3f"),
            *Facts.PresentationStyleId.ToString(),*Activation.ToString(),Remaining);
    }
}

void UDivineBeastsDevelopmentCastingWorldSubsystem::StopCastAudio(
    UGamePlatformAbilityCastComponent* Component,const FGuid ExpectedActivation)
{
    auto* Binding=Bindings.Find(Component);
    if(!Binding || (ExpectedActivation.IsValid() && Binding->AudioActivationId!=ExpectedActivation)) return;
    const FBinding Old=*Binding;
    // 保留Activation防止已结束窗口被资源通知重播；下一次真实施法使用新的Activation。
    Binding->AudioRequestId.Invalidate(); Binding->AudioAdapter.Reset(); Binding->AudioSemantic={};
    Binding->AudioWorldGeneration=0; Binding->bSubmittingAudio=false; Binding->AudioStopTimer.Invalidate();
    ReleaseCastAudio(Old);
}

void UDivineBeastsDevelopmentCastingWorldSubsystem::ReleaseCastAudio(const FBinding& Old)
{
    FTimerHandle Timer=Old.AudioStopTimer;
    if(auto* World=GetWorld()) World->GetTimerManager().ClearTimer(Timer);
    if(Old.AudioRequestId.IsValid() && !Old.bSubmittingAudio)
        if(auto* Adapter=Old.AudioAdapter.Get())
        {
            FGamePlatformPresentationRequest Cancel; Cancel.RequestId=Old.AudioRequestId;
            Cancel.SemanticTag=Old.AudioSemantic; Cancel.WorldGeneration=Old.AudioWorldGeneration;
            Cancel.Context.ProjectId=FDivineBeastsProjectCatalog::GetProjectId();
            Cancel.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;
            Adapter->Submit(Cancel);
            UE_LOG(LogTemp,Log,TEXT("SSLM_CAST_AUDIO Stage=End Activation=%s"),*Old.AudioActivationId.ToString());
        }
}

void UDivineBeastsDevelopmentCastingWorldSubsystem::StopCast(UGamePlatformAbilityCastComponent* Component,FGuid ExpectedOperationId)
{
    if(!Component) return;
    auto* Binding=Bindings.Find(Component); if(!Binding) return;
    if(ExpectedOperationId.IsValid() && Binding->OperationId!=ExpectedOperationId) return;
    const FBinding Old=*Binding;
    Binding->ActivationId.Invalidate(); Binding->OperationId.Invalidate(); Binding->VFXRequestId.Invalidate();
    Binding->Anim.Reset(); Binding->Montage.Reset(); Binding->SkeletalMesh.Reset(); Binding->CastAnchor.Reset();
    Binding->VFXAdapter.Reset(); Binding->VFXWorldGeneration=0; Binding->MontageInstanceId=INDEX_NONE;
    Binding->bStartingMontage=false; Binding->bSubmittingVFX=false; Binding->StyleName=NAME_None; Binding->StopTimer.Invalidate();
    ReleaseCastResources(Old);
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::ReleaseCastResources(const FBinding& Old)
{
    FTimerHandle Timer=Old.StopTimer;
    if(auto* World=GetWorld()) World->GetTimerManager().ClearTimer(Timer);
    // 启动栈内尚未确认实例，不能按资产误停同步重播者；由对应Play返回栈补做唯一实例清理。
    if(!Old.bStartingMontage && Old.MontageInstanceId!=INDEX_NONE && Old.Anim.IsValid() && Old.Montage.IsValid())
    {
        const auto* Instance=Old.Anim->GetActiveInstanceForMontage(Old.Montage.Get());
        if(Instance && Instance->GetInstanceID()==Old.MontageInstanceId)
            Old.Anim->Montage_Stop(0.12f,Old.Montage.Get());
    }
    if(Old.VFXRequestId.IsValid() && !Old.bSubmittingVFX)
        if(auto* Adapter=Old.VFXAdapter.Get())
        {
            FGamePlatformPresentationRequest Cancel; Cancel.RequestId=Old.VFXRequestId;
            Cancel.SemanticTag=CastSemantic(Old.StyleName); Cancel.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;
            Cancel.Context.ProjectId=FDivineBeastsProjectCatalog::GetProjectId(); Cancel.WorldGeneration=Old.VFXWorldGeneration;
            Adapter->Submit(Cancel);
        }
    if(Old.CastAnchor.IsValid()) Old.CastAnchor->DestroyComponent();
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::DetachObservers(UGamePlatformAbilityCastComponent* Component,const FBinding& Binding)
{
    if(IsValid(Component))
    {
        Component->OnCastFactsChanged().Remove(Binding.FactsHandle);
        if(auto* Owner=Component->GetOwner()) Owner->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleActorEndPlay);
    }
    if(auto* Identity=Binding.Identity.Get())
    {
        Identity->OnIdentityChanged().Remove(Binding.IdentityHandle);
        Identity->OnReadinessChanged().Remove(Binding.ReadinessHandle);
    }
    if(auto* Mesh=Binding.Mesh.Get()) Mesh->OnAnimInitialized.RemoveDynamic(this,&ThisClass::HandleAnimInitialized);
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::RefreshFacts()
{
    if(bClosing) return;
    TArray<TWeakObjectPtr<UGamePlatformAbilityCastComponent>> Components; Bindings.GetKeys(Components);
    for(auto& Weak:Components) if(auto* Component=Weak.Get()) HandleFacts(Component,Component->GetCastFacts());
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::HandleAnimInitialized() { RefreshFacts(); }
void UDivineBeastsDevelopmentCastingWorldSubsystem::HandlePack(const FDivineBeastsPresentationContentPackHandle&,EDivineBeastsPresentationContentPackState State,const FString&)
{ if(State==EDivineBeastsPresentationContentPackState::Active) RefreshFacts(); }
void UDivineBeastsDevelopmentCastingWorldSubsystem::HandleActorEndPlay(AActor* Actor,EEndPlayReason::Type)
{
    auto* Component=Actor->FindComponentByClass<UGamePlatformAbilityCastComponent>();
    const auto* Binding=Bindings.Find(Component); if(!Binding) return;
    const FBinding Old=*Binding; Bindings.Remove(Component);
    DetachObservers(Component,Old); ReleaseCastAudio(Old); ReleaseCastResources(Old);
}
void UDivineBeastsDevelopmentCastingWorldSubsystem::Deinitialize()
{
    if(bClosing) return;
    bClosing=true; LifetimeToken=FGuid::NewGuid(); bReady=false;
    if(auto* Adapter=Presentation.Get()) Adapter->OnContentPackChanged().Remove(PackHandle);
    if(auto* World=GetWorld()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    // 整表先退休、全量解绑，再清资源；取消通知内销毁角色也不会使下一条清理依赖FindChecked。
    auto RetiredBindings=MoveTemp(Bindings); Bindings.Reset();
    for(const auto& Entry:RetiredBindings) DetachObservers(Entry.Key.Get(),Entry.Value);
    for(const auto& Entry:RetiredBindings) { ReleaseCastAudio(Entry.Value); ReleaseCastResources(Entry.Value); }
    const auto RetiredLease=VisualLease; VisualLease={};
    if(auto* World=GetWorld(); World && World->GetGameInstance())
        if(auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance()); Data && RetiredLease.IsValid()) Data->ReleaseResources(RetiredLease);
    Presentation.Reset(); PlatformPresentation.Reset(); Super::Deinitialize();
}
