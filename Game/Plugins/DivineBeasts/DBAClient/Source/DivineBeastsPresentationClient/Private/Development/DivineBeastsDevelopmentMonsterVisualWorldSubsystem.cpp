// 神兽联盟开发石灵只读视觉：复制生命事件→Monolith蓝图和世界空间生命标签；Data保留唯一加载所有权。
#include "Development/DivineBeastsDevelopmentMonsterVisualWorldSubsystem.h"
#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 只允许项目公共内容包的本轮稳定类；网络复制不携带任意类路径。 */
const FSoftObjectPath StoneVisual(TEXT("/DBAContentPack_Common/Development/Monsters/BP_DBA_AncientStoneSpiritVisual.BP_DBA_AncientStoneSpiritVisual_C"));
/** 字体归UI内容包，Common装饰BP保留空Text槽；项目客户端经同一Data租约组合，不让内容包互相硬引。 */
const FSoftObjectPath StoneLabelFont(TEXT("/DBAUIPack_Core/UI/Fonts/Development/F_DBA_DevelopmentMonsterLabel.F_DBA_DevelopmentMonsterLabel"));
/** 不能以Health单调减少推断先后；完整值必须与怪物当前投影相等，未来真实治疗同样允许。 */
bool SameLifeValues(const FDivineBeastsDevelopmentMonsterLifeState& A,const FDivineBeastsDevelopmentMonsterLifeState& B)
{
    return A.AvatarGeneration==B.AvatarGeneration && A.Health==B.Health && A.MaxHealth==B.MaxHealth && A.bDead==B.bDead;
}
}
bool UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* World=Cast<UWorld>(Outer);
    return !IsRunningCommandlet() && World && World->IsGameWorld() && World->GetNetMode()!=NM_DedicatedServer &&
        ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess();
}
void UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
    if(bClosing || SpawnHandle.IsValid() || Lease.IsValid()) return;
    Super::OnWorldBeginPlay(World); LifetimeToken=FGuid::NewGuid();
    if(!World.GetGameInstance() || World.bIsTearingDown || !ShouldCreateSubsystem(&World)) return;
    SpawnHandle=World.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&ThisClass::ObserveActor));
    for(TActorIterator<ADivineBeastsDevelopmentMonster> It(&World);It;++It) ObserveActor(*It);
    auto* Data=IGamePlatformDataService::Get(*World.GetGameInstance());
    if(!Data){UE_LOG(LogTemp,Warning,TEXT("开发石灵外观缺Data服务；怪物伤害资格不受影响"));return;}
    const FGuid Token=LifetimeToken; TWeakObjectPtr<UDivineBeastsDevelopmentMonsterVisualWorldSubsystem> WeakThis(this);
    const TWeakObjectPtr<UWorld> WeakWorld(&World);
    FGamePlatformResult Result;
    // Data公开合同保证完成总是延后；本次成功必须同时保留BP与UIFont，不能用第二个加载管理器补字体。
    Lease=Data->AcquireResources({StoneVisual,StoneLabelFont},EGamePlatformDataLifetime::World,this,
        [WeakThis,WeakWorld,Token](const FGamePlatformDataLease& CompletedLease,const FGamePlatformResult& Completed)
        {
            auto* Self=WeakThis.Get();
            auto* CurrentWorld=WeakWorld.Get();
            if(!Self || Self->bClosing || Self->LifetimeToken!=Token || !CurrentWorld || CurrentWorld->bIsTearingDown ||
                Self->GetWorld()!=CurrentWorld || !Self->Lease.IsValid() || Self->Lease.LeaseId!=CompletedLease.LeaseId ||
                Self->Lease.ScopeId!=CompletedLease.ScopeId || Self->Lease.Generation!=CompletedLease.Generation ||
                Self->Lease.IssuerProof!=CompletedLease.IssuerProof) return;
            const TStrongObjectPtr<UDivineBeastsDevelopmentMonsterVisualWorldSubsystem> KeepCompletionOwner(Self);
            const TStrongObjectPtr<UWorld> KeepCompletionWorld(CurrentWorld);
            auto* Class=Cast<UClass>(StoneVisual.ResolveObject());
            auto* Font=Cast<UFont>(StoneLabelFont.ResolveObject());
            Self->bReady=Completed.IsSuccess() && IsValid(Class) && Class->IsChildOf(AActor::StaticClass()) &&
                IsValid(Font) && Font->FontCacheType==EFontCacheType::Offline;
            if(!Self->bReady){UE_LOG(LogTemp,Warning,TEXT("开发石灵资源预热或Actor/离线字体校验失败：%s"),*Completed.Code.ToString());return;}
            TArray<TWeakObjectPtr<ADivineBeastsDevelopmentMonster>> Monsters; Self->Visuals.GetKeys(Monsters);
            for(auto& Weak:Monsters)
            {
                if(Self->bClosing || Self->LifetimeToken!=Token || Self->GetWorld()!=CurrentWorld || CurrentWorld->bIsTearingDown) break;
                if(auto* Monster=Weak.Get()) Self->ApplyLife(Monster,Monster->GetLifeState());
            }
        },Result);
    if(!Result.IsSuccess()) UE_LOG(LogTemp,Warning,TEXT("开发石灵资源请求未接纳：%s"),*Result.Code.ToString());
}
void UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::ObserveActor(AActor* Actor)
{
    auto* Monster=Cast<ADivineBeastsDevelopmentMonster>(Actor);
    auto* World=GetWorld();
    if(bClosing || !IsValid(Monster) || Monster->IsActorBeingDestroyed() || !IsValid(World) || World->bIsTearingDown ||
        Monster->GetWorld()!=World || Visuals.Contains(Monster)) return;
    TWeakObjectPtr<ADivineBeastsDevelopmentMonster> WeakMonster(Monster);
    FVisual Visual;
    Visual.ObservationId=FGuid::NewGuid();
    Visual.LifeHandle=Monster->OnLifeStateChanged().AddWeakLambda(this,[this,WeakMonster](const FDivineBeastsDevelopmentMonsterLifeState& State)
        { if(auto* Current=WeakMonster.Get()) ApplyLife(Current,State); });
    Visuals.Add(Monster,Visual);
    Monster->OnEndPlay.AddUniqueDynamic(this,&ThisClass::HandleActorEndPlay);
    ApplyLife(Monster,Monster->GetLifeState());
}
bool UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::IsObservationCurrent(ADivineBeastsDevelopmentMonster* Monster,
    UWorld* World,const FGuid& WorldToken,const FGuid& ObservationId) const
{
    if(bClosing || !bReady || LifetimeToken!=WorldToken || !IsValid(World) || World->bIsTearingDown || GetWorld()!=World ||
        !IsValid(Monster) || Monster->IsActorBeingDestroyed() || Monster->GetWorld()!=World) return false;
    const auto* Current=Visuals.Find(Monster);
    return Current && Current->ObservationId==ObservationId;
}
bool UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::IsVisualOperationCurrent(ADivineBeastsDevelopmentMonster* Monster,
    UWorld* World,const FGuid& WorldToken,const FGuid& ObservationId,const FGuid& UpdateId,AActor* Actor,
    const FDivineBeastsDevelopmentMonsterLifeState& State) const
{
    if(!IsObservationCurrent(Monster,World,WorldToken,ObservationId) || !IsValid(Actor) || Actor->IsActorBeingDestroyed() ||
        Actor->GetWorld()!=World || !SameLifeValues(State,Monster->GetLifeState())) return false;
    const auto* Current=Visuals.Find(Monster);
    return Current && !Current->bCreatingActor && Current->UpdateId==UpdateId && Current->Actor.Get()==Actor &&
        Current->Generation==State.AvatarGeneration;
}
void UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::ApplyLife(ADivineBeastsDevelopmentMonster* Monster,const FDivineBeastsDevelopmentMonsterLifeState& Incoming)
{
    UWorld* World=GetWorld();
    if(bClosing || !bReady || !IsValid(World) || World->bIsTearingDown || !IsValid(Monster) || Monster->IsActorBeingDestroyed() ||
        Monster->GetWorld()!=World || Incoming.AvatarGeneration<=0 || !SameLifeValues(Incoming,Monster->GetLifeState())) return;
    // 强引用只防止同步回调中的GC，不替代世界/观察/操作身份；每个外部边界仍须重新核查。
    const TStrongObjectPtr<UDivineBeastsDevelopmentMonsterVisualWorldSubsystem> KeepSelf(this);
    const TStrongObjectPtr<UWorld> KeepWorld(World);
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> KeepMonster(Monster);
    auto* Font=Cast<UFont>(StoneLabelFont.ResolveObject());
    if(!IsValid(Font) || Font->FontCacheType!=EFontCacheType::Offline) return;
    const TStrongObjectPtr<UFont> KeepFont(Font);
    const FGuid WorldToken=LifetimeToken;
    const TWeakObjectPtr<ADivineBeastsDevelopmentMonster> WeakMonster(Monster);
    FGuid ObservationId;
    AActor* Actor=nullptr;
    {
        auto* Current=Visuals.Find(WeakMonster);
        if(!Current || Current->bCreatingActor || Incoming.AvatarGeneration<Current->Generation) return;
        ObservationId=Current->ObservationId;
        Actor=Current->Actor.Get();
        if(!Actor) Current->bCreatingActor=true;
    }
    if(!Actor)
    {
        // Map中先登记创建栅栏；Spawn/附着同步触发的真实受伤只推进Monster事实，回栈再取最新值。
        const auto AbortCreation=[this,WeakMonster,ObservationId](AActor* Created)
        {
            {
                auto* Current=Visuals.Find(WeakMonster);
                if(Current && Current->ObservationId==ObservationId)
                {
                    if(Current->Actor.Get()==Created) Current->Actor.Reset();
                    Current->bCreatingActor=false;
                }
            }
            if(IsValid(Created) && !Created->IsActorBeingDestroyed()) Created->Destroy();
        };
        auto* Class=Cast<UClass>(StoneVisual.ResolveObject());
        if(!IsValid(Class) || !Class->IsChildOf(AActor::StaticClass())) {AbortCreation(nullptr);return;}
        FActorSpawnParameters Spawn; Spawn.Owner=Monster; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Actor=World->SpawnActor<AActor>(Class,Monster->GetActorTransform(),Spawn);
        if(!IsObservationCurrent(Monster,World,WorldToken,ObservationId) || !IsValid(Actor) || Actor->IsActorBeingDestroyed())
        {AbortCreation(Actor);return;}
        const TStrongObjectPtr<AActor> KeepCreatedActor(Actor);
        {
            auto* Current=Visuals.Find(WeakMonster);
            Current->Actor=Actor;
        }
        const auto CreationIsCurrent=[this,Monster,World,WorldToken,ObservationId,Actor]
        {
            if(!IsObservationCurrent(Monster,World,WorldToken,ObservationId) || !IsValid(Actor) || Actor->IsActorBeingDestroyed()) return false;
            const auto* Current=Visuals.Find(Monster);
            return Current && Current->bCreatingActor && Current->Actor.Get()==Actor;
        };
        Actor->SetActorEnableCollision(false);
        if(!CreationIsCurrent()) {AbortCreation(Actor);return;}
        Actor->SetReplicates(false);
        if(!CreationIsCurrent()) {AbortCreation(Actor);return;}
        Actor->AttachToActor(Monster,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        if(!CreationIsCurrent()) {AbortCreation(Actor);return;}
        Actor->SetActorRelativeLocation(FVector(0,0,-Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
        if(!CreationIsCurrent()) {AbortCreation(Actor);return;}
        {
            auto* Current=Visuals.Find(WeakMonster);
            Current->bCreatingActor=false;
        }
    }
    if(!IsObservationCurrent(Monster,World,WorldToken,ObservationId) || !IsValid(Actor) || Actor->IsActorBeingDestroyed()) return;
    const TStrongObjectPtr<AActor> KeepActor(Actor);
    const auto State=Monster->GetLifeState();
    if(State.AvatarGeneration<=0) return;
    const FGuid UpdateId=FGuid::NewGuid();
    bool bHit=false;
    FTimerHandle OldHitTimer;
    {
        auto* Current=Visuals.Find(WeakMonster);
        if(State.AvatarGeneration<Current->Generation) return;
        bHit=Current->Generation==State.AvatarGeneration && !State.bDead && State.Health<Current->PreviousHealth;
        Current->UpdateId=UpdateId;
        Current->PreviousHealth=State.Health;
        Current->Generation=State.AvatarGeneration;
        OldHitTimer=Current->HitTimer;
        Current->HitTimer.Invalidate();
    }
    World->GetTimerManager().ClearTimer(OldHitTimer);
    const auto IsCurrent=[this,Monster,World,WorldToken,ObservationId,UpdateId,Actor,State]
        {return IsVisualOperationCurrent(Monster,World,WorldToken,ObservationId,UpdateId,Actor,State);};
    if(!IsCurrent()) return;
    // 训练原型死亡以蹲低石体表示；不伪称已有正式骨骼死亡动画，重生恢复原尺寸。
    Actor->SetActorRelativeScale3D(State.bDead ? FVector(1.0,1.0,0.25) : FVector::OneVector);
    if(!IsCurrent()) return;
    TArray<UTextRenderComponent*> Labels; Actor->GetComponents(Labels);
    const FText NameText=Monster->GetMonsterDisplayName();
    const FText LifeText=State.bDead ? FText::FromString(TEXT("已击败 · 等待重生")) :
        FText::FromString(FString::Printf(TEXT("生命 %.0f / %.0f"),State.Health,State.MaxHealth));
    bool bNameReady=false,bLifeReady=false;
    for(auto* Label:Labels)
    {
        if(!IsValid(Label) || Label->GetOwner()!=Actor) continue;
        const bool bName=Label->ComponentHasTag(TEXT("DBA.Monster.NameLabel"));
        const bool bLife=Label->ComponentHasTag(TEXT("DBA.Monster.LifeLabel"));
        if(!bName && !bLife) continue;
        Label->SetFont(Font);
        if(!IsCurrent() || !IsValid(Label) || Label->GetOwner()!=Actor) return;
        Label->SetText(bName ? NameText : LifeText);
        if(!IsCurrent() || !IsValid(Label) || Label->GetOwner()!=Actor) return;
        bNameReady|=bName && Label->Font.Get()==Font && Label->Text.EqualTo(NameText);
        bLifeReady|=bLife && Label->Font.Get()==Font && Label->Text.EqualTo(LifeText);
    }
    if(bHit)
    {
        Actor->SetActorRelativeScale3D(FVector(0.97,0.97,0.97));
        // TransformUpdated可同步Deinitialize/Destroy；先核查再创建本地Timer，绝不使用回调前的Map元素引用。
        if(!IsCurrent()) return;
        const TWeakObjectPtr<AActor> WeakActor(Actor);
        const TWeakObjectPtr<UWorld> WeakWorld(World);
        FTimerHandle NewHitTimer;
        World->GetTimerManager().SetTimer(NewHitTimer,FTimerDelegate::CreateWeakLambda(this,
            [this,WeakActor,WeakMonster,WeakWorld,WorldToken,ObservationId,UpdateId,State]()
        {
            auto* CurrentWorld=WeakWorld.Get();
            auto* CurrentMonster=WeakMonster.Get();
            auto* CurrentActor=WeakActor.Get();
            if(!IsVisualOperationCurrent(CurrentMonster,CurrentWorld,WorldToken,ObservationId,UpdateId,CurrentActor,State) || State.bDead) return;
            const TStrongObjectPtr<UDivineBeastsDevelopmentMonsterVisualWorldSubsystem> KeepTimerOwner(this);
            const TStrongObjectPtr<UWorld> KeepTimerWorld(CurrentWorld);
            const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> KeepTimerMonster(CurrentMonster);
            const TStrongObjectPtr<AActor> KeepTimerActor(CurrentActor);
            {
                auto* Current=Visuals.Find(WeakMonster);
                Current->HitTimer.Invalidate();
            }
            // 最后一步允许再次同步退出；此后不再读写任何Map元素或创建Timer。
            CurrentActor->SetActorRelativeScale3D(FVector::OneVector);
        }),0.12f,false);
        if(!IsCurrent()) {World->GetTimerManager().ClearTimer(NewHitTimer);return;}
        {
            auto* Current=Visuals.Find(WeakMonster);
            Current->HitTimer=NewHitTimer;
        }
    }
    if(IsCurrent() && bNameReady && bLifeReady)
    {
        bool bLogReady=false;
        {
            auto* Current=Visuals.Find(WeakMonster);
            bLogReady=!Current->bVisualReadyLogged;
            Current->bVisualReadyLogged=true;
        }
        if(bLogReady) UE_LOG(LogTemp,Display,TEXT("DevelopmentMonsterVisualReady gen=%d HP=%.0f/%.0f"),
            State.AvatarGeneration,State.Health,State.MaxHealth);
    }
}
void UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::RemoveVisual(ADivineBeastsDevelopmentMonster* Monster)
{
    FVisual Old; if(!Visuals.RemoveAndCopyValue(Monster,Old)) return;
    if(auto* World=GetWorld()) World->GetTimerManager().ClearTimer(Old.HitTimer);
    if(IsValid(Monster))
    { Monster->OnLifeStateChanged().Remove(Old.LifeHandle); Monster->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleActorEndPlay); }
    if(Old.Actor.IsValid()) Old.Actor->Destroy();
}
void UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::HandleActorEndPlay(AActor* Actor,EEndPlayReason::Type)
{ if(auto* Monster=Cast<ADivineBeastsDevelopmentMonster>(Actor)) RemoveVisual(Monster); }
void UDivineBeastsDevelopmentMonsterVisualWorldSubsystem::Deinitialize()
{
    if(bClosing) return;
    const TStrongObjectPtr<UWorld> KeepWorld(GetWorld());
    bClosing=true; bReady=false; LifetimeToken=FGuid::NewGuid();
    if(auto* World=GetWorld()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    SpawnHandle.Reset();
    auto OldVisuals=MoveTemp(Visuals); Visuals.Reset();
    // 在任何Destroy回调前撤销本地可操作身份及Lease引用；重入退出不能重复释放或恢复观察记录。
    const auto OldLease=Lease; Lease={};
    for(auto& Pair:OldVisuals)
    {
        if(auto* World=GetWorld()) World->GetTimerManager().ClearTimer(Pair.Value.HitTimer);
        if(auto* Monster=Pair.Key.Get())
        { Monster->OnLifeStateChanged().Remove(Pair.Value.LifeHandle); Monster->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleActorEndPlay); }
        if(Pair.Value.Actor.IsValid()) Pair.Value.Actor->Destroy();
    }
    if(auto* World=GetWorld();World && World->GetGameInstance())
        if(auto* Data=IGamePlatformDataService::Get(*World->GetGameInstance());Data && OldLease.IsValid()) Data->ReleaseResources(OldLease);
    Super::Deinitialize();
}
