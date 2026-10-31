// 门只发布活动事实；可选旋涡由客户端World经项目内容预载→平台Presentation→唯一VFX链处理。
#include "World/DivineBeastsArenaEntranceVFXWorldSubsystem.h"
#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "NativeGameplayTags.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(ArenaEntranceVortexTag,"Presentation.Environment.ArenaEntrance.Vortex");
const FName PackId(TEXT("DBA.Village.ArenaEntrance.Vortex"));
const FName DefinitionId(TEXT("divinebeasts.vfx.village_arena_gate_vortex@1"));
}

FGameplayTag UDivineBeastsArenaEntranceVFXWorldSubsystem::GetVortexSemantic(){return ArenaEntranceVortexTag;}

FDivineBeastsPresentationContentPackFragment UDivineBeastsArenaEntranceVFXWorldSubsystem::BuildContentFragment()
{
    FDivineBeastsPresentationContentPackFragment Fragment;
    Fragment.ContentPackId=PackId;Fragment.Revision=1;Fragment.LifecycleScope=EGamePlatformPresentationContextScope::World;
    Fragment.bRequiredPreload=true;Fragment.LogicalPreloadDefinitionIds.Add(DefinitionId);
    auto& Catalog=Fragment.CatalogFragment;
    Catalog.FragmentId=TEXT("DBA.Presentation.Village.ArenaEntrance.Vortex");Catalog.Revision=1;
    Catalog.Scope=EGamePlatformPresentationCatalogScope::ContentPack;Catalog.OwnerScopeId=PackId;Catalog.LifecycleScope=Fragment.LifecycleScope;
    FGamePlatformPresentationCatalogEntry Entry;
    Entry.EntryId=TEXT("DBA.Village.ArenaEntrance.Vortex.Visual");Entry.SemanticTag=ArenaEntranceVortexTag;
    Entry.ContextQuery.ContentPackId=PackId;Entry.ProviderChannel=TEXT("VFX");Entry.DefinitionId=DefinitionId;
    Entry.Scope=EGamePlatformPresentationCatalogScope::ContentPack;Entry.ContentRevision=TEXT("1");
    Catalog.Entries.Add(Entry);return Fragment;
}

bool UDivineBeastsArenaEntranceVFXWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* World=Cast<UWorld>(Outer);
    return World&&(World->WorldType==EWorldType::Game||World->WorldType==EWorldType::PIE)&&
        World->GetNetMode()!=NM_DedicatedServer&&!IsRunningCommandlet();
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);bClosing=false;OwnerToken=FGuid::NewGuid();
    SpawnHandle=GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&ThisClass::ObserveActor));
    BindGameInstanceEvents();
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::BindGameInstanceEvents()
{
    if(GameInstance.IsValid())return;
    auto* Instance=GetWorld()?GetWorld()->GetGameInstance():nullptr;if(!Instance)return;
    GameInstance=Instance;
    PlayerAddedHandle=Instance->OnLocalPlayerAddedEvent.AddWeakLambda(this,[this](ULocalPlayer*){RefreshOwner();});
    PlayerRemovedHandle=Instance->OnLocalPlayerRemovedEvent.AddWeakLambda(this,[this](ULocalPlayer*){RefreshOwner();});
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);if(bClosing)return;
    bStarted=true;BindGameInstanceEvents();
    for(TActorIterator<ADivineBeastsArenaEntrance> It(&World);It;++It)ObserveActor(*It);
    RefreshOwner();
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::RefreshOwner()
{
    if(bClosing||!bStarted||!GetWorld()||GetWorld()->bIsTearingDown)return;
    auto* Instance=GameInstance.Get();auto* Player=Instance?Instance->GetFirstGamePlayer():nullptr;
    if(Player&&Player->GetWorld()!=GetWorld())Player=nullptr;
    if(PrimaryPlayer.Get()==Player&&Presentation.IsValid()&&PlatformPresentation.IsValid()){EnsureContentPack();return;}
    ReleasePresentation();
    // 取消/撤包是同步外部边界；嵌套切换已经绑定后继时，本栈不能再覆盖。
    if(bClosing||PrimaryPlayer.IsValid())return;
    Instance=GameInstance.Get();Player=Instance?Instance->GetFirstGamePlayer():nullptr;
    if(!Player||Player->GetWorld()!=GetWorld())return;
    auto* Project=Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>();
    auto* Platform=Player->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    if(!Project||!Platform)return;
    PrimaryPlayer=Player;Presentation=Project;PlatformPresentation=Platform;OwnerToken=FGuid::NewGuid();
    const FGuid Expected=OwnerToken;
    PackChangedHandle=Project->OnContentPackChanged().AddWeakLambda(this,[this,Expected](const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State,const FString& Error){HandlePack(Handle,State,Error,Expected);});
    EnsureContentPack();
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::EnsureContentPack()
{
    for(auto It=Bindings.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
    if(bClosing||!bStarted||Bindings.IsEmpty()||bActivationAttempted||!Presentation.IsValid()||!PlatformPresentation.IsValid())return;
    bActivationAttempted=true;const FGuid Expected=OwnerToken;
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Project(Presentation.Get());
    FString Error;
    const auto Handle=Project->ActivateContentPack(BuildContentFragment(),Error,&PackHandle);
    if(bClosing||OwnerToken!=Expected)
    {if(Handle.IsValid())Project->DeactivateContentPack(Handle);return;}
    if(!Handle.IsValid())
    {
        PackHandle={};bPackReady=false;
        UE_LOG(LogTemp,Warning,TEXT("竞技门旋涡内容注册/预载未受理：%s；入口玩法继续可用。"),*Error);
    }
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::HandlePack(const FDivineBeastsPresentationContentPackHandle& Handle,
    EDivineBeastsPresentationContentPackState State,const FString& Error,const FGuid& Expected)
{
    if(bClosing||Expected!=OwnerToken||!PackHandle.IsValid()||Handle.Id!=PackHandle.Id||Handle.ScopeId!=PackHandle.ScopeId||Handle.Generation!=PackHandle.Generation)return;
    if(State==EDivineBeastsPresentationContentPackState::Active)
    {
        FString StateError;
        bPackReady=Presentation.IsValid()&&Presentation->GetContentPackState(Handle,StateError)==EDivineBeastsPresentationContentPackState::Active;
        if(bPackReady)SubmitAll();return;
    }
    if(State==EDivineBeastsPresentationContentPackState::Loading)return;
    bPackReady=false;PackHandle={};
    TArray<FBinding> Previous;
    for(auto& Pair:Bindings){Previous.Add(Pair.Value);Pair.Value.Request={};Pair.Value.Adapter.Reset();Pair.Value.bSubmitting=false;Pair.Value.bFailed=true;}
    for(auto& Binding:Previous)CancelRequest(Binding);
    UE_LOG(LogTemp,Warning,TEXT("竞技门旋涡内容未激活或已撤销：%s；未重试或影响交互。"),*Error);
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::ObserveActor(AActor* Actor)
{
    if(bClosing||!IsValid(Actor)||Actor->GetWorld()!=GetWorld()||Actor->IsActorBeingDestroyed()||!Actor->IsA<ADivineBeastsArenaEntrance>()||Bindings.Contains(Actor))return;
    Bindings.Add(Actor,FBinding{});Actor->OnEndPlay.AddUniqueDynamic(this,&ThisClass::HandleActorEndPlay);
    if(bStarted){RefreshOwner();if(bPackReady)SubmitActor(Actor);}
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::SubmitAll()
{
    TArray<TWeakObjectPtr<AActor>> Actors;Bindings.GetKeys(Actors);
    for(const auto& Actor:Actors)if(Actor.IsValid())SubmitActor(Actor.Get());
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::SubmitActor(AActor* Actor)
{
    auto* Binding=Bindings.Find(Actor);auto* Platform=PlatformPresentation.Get();auto* Player=PrimaryPlayer.Get();
    if(bClosing||!bPackReady||!Binding||Binding->Request.RequestId.IsValid()||Binding->bFailed||!Platform||!Player||
        Player->GetWorld()!=GetWorld()||!IsValid(Actor)||Actor->IsActorBeingDestroyed()||!GetWorld()||GetWorld()->bIsTearingDown)return;
    auto* Root=Actor->GetRootComponent();if(!Root||!Root->IsRegistered())return;
    const TStrongObjectPtr<UDivineBeastsArenaEntranceVFXWorldSubsystem> KeepService(this);
    const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> KeepPlatform(Platform);
    const FGuid Expected=OwnerToken;
    FGamePlatformPresentationRequest Request;Request.RequestId=FGuid::NewGuid();Request.SemanticTag=ArenaEntranceVortexTag;
    Request.ContextId=Actor->GetFName();Request.Context.ContentPackId=PackId;
    Request.SourceLocation=Actor->GetActorTransform().TransformPosition(FVector(0,0,210));
    Request.SourceRotation=Actor->GetActorRotation();Request.AttachComponent=Root;
    Request.Lifetime=EGamePlatformPresentationLifetime::Persistent;Request.Priority=EGamePlatformPresentationPriority::Low;
    Request.WorldGeneration=Platform->GetWorldGeneration();Request.RequestGeneration=1;
    Binding->Request=Request;Binding->Adapter=Platform;Binding->bSubmitting=true;
    FBinding Snapshot=*Binding;
    const auto Result=Platform->Submit(Request);
    // 原提交栈可能同步触发Actor退出、撤包或Primary更换；只处理值快照，不保留TMap地址跨外调。
    Binding=Bindings.Find(Actor);
    if(bClosing||Expected!=OwnerToken||!Binding||Binding->Request.RequestId!=Request.RequestId)
    {if(Result==EGamePlatformPresentationSubmitResult::Submitted){Snapshot.bSubmitting=false;CancelRequest(Snapshot);}return;}
    Binding->bSubmitting=false;
    if(Result!=EGamePlatformPresentationSubmitResult::Submitted)
    {
        Binding->Request={};Binding->Adapter.Reset();Binding->bFailed=true;
        UE_LOG(LogTemp,Warning,TEXT("竞技门旋涡请求未受理，状态=%d；不会自动循环播放重试。"),static_cast<int32>(Result));
    }
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::CancelRequest(FBinding Binding)
{
    if(Binding.bSubmitting||!Binding.Request.RequestId.IsValid())return;
    const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Adapter(Binding.Adapter.Get());
    if(Adapter.IsValid())
    {
        // 撤包后的清理仍携原ID、World代次及已知提供者；不会重新解析到其他内容或操作新世界。
        Binding.Request.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;
        Binding.Request.ProviderChannel=TEXT("VFX");Binding.Request.DefinitionId=DefinitionId;
        Adapter->Submit(Binding.Request);
    }
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::RemoveActor(AActor* Actor)
{
    FBinding Removed;if(!Bindings.RemoveAndCopyValue(Actor,Removed))return;
    if(IsValid(Actor))Actor->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleActorEndPlay);
    CancelRequest(Removed);
    if(Bindings.IsEmpty())ReleasePresentation();
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::HandleActorEndPlay(AActor* Actor,EEndPlayReason::Type){RemoveActor(Actor);}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::ReleasePresentation()
{
    OwnerToken=FGuid::NewGuid();bPackReady=false;bActivationAttempted=false;
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Project(Presentation.Get());
    const auto OldPack=PackHandle;const auto OldHandle=PackChangedHandle;
    PackHandle={};PackChangedHandle.Reset();Presentation.Reset();PlatformPresentation.Reset();PrimaryPlayer.Reset();
    TArray<FBinding> Previous;
    for(auto& Pair:Bindings){Previous.Add(Pair.Value);Pair.Value.Request={};Pair.Value.Adapter.Reset();Pair.Value.bSubmitting=false;Pair.Value.bFailed=false;}
    if(Project.IsValid())Project->OnContentPackChanged().Remove(OldHandle);
    for(auto& Binding:Previous)CancelRequest(Binding);
    if(Project.IsValid()&&OldPack.IsValid())Project->DeactivateContentPack(OldPack);
}
void UDivineBeastsArenaEntranceVFXWorldSubsystem::Deinitialize()
{
    if(bClosing)return;bClosing=true;bStarted=false;
    if(GetWorld())GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);SpawnHandle.Reset();
    if(auto* Instance=GameInstance.Get()){Instance->OnLocalPlayerAddedEvent.Remove(PlayerAddedHandle);Instance->OnLocalPlayerRemovedEvent.Remove(PlayerRemovedHandle);}
    GameInstance.Reset();PlayerAddedHandle.Reset();PlayerRemovedHandle.Reset();
    for(const auto& Pair:Bindings)if(auto* Actor=Pair.Key.Get())Actor->OnEndPlay.RemoveDynamic(this,&ThisClass::HandleActorEndPlay);
    ReleasePresentation();Bindings.Reset();Super::Deinitialize();
}
