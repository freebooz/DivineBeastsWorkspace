// 装备人工台：自己的Actor/组件；Editor服务器桥不进入普通Client依赖。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Components/GamePlatformEquipmentComponent.h"
#include "Components/GamePlatformEquipmentVisualComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Definitions/GamePlatformEquipmentDefinition.h"
#include "Definitions/GamePlatformEquipmentVisualDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#if WITH_EDITOR
#include "Components/GamePlatformEquipmentServerComponent.h"
#include "Interfaces/GamePlatformEquipmentPersistencePort.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#endif
namespace DivineBeasts::Demo
{
#if WITH_EDITOR
class FManualEquipmentPort final : public IGamePlatformEquipmentPersistencePort
{
public:
    FGamePlatformEquipmentLoadCompletion Load;
    FGamePlatformEquipmentMutationCompletion Mutation;
    FGuid LastOperation;
    bool BeginLoadEquipment(const FString&,const FString&,FGamePlatformEquipmentLoadCompletion C) override{Load=MoveTemp(C);return true;}
    bool BeginEquip(const FString&,const FGamePlatformEquipRequest& R,FGamePlatformEquipmentMutationCompletion C) override{LastOperation=R.OperationId;Mutation=MoveTemp(C);return true;}
    bool BeginUnequip(const FString&,const FGamePlatformUnequipRequest& R,FGamePlatformEquipmentMutationCompletion C) override{LastOperation=R.OperationId;Mutation=MoveTemp(C);return true;}
    bool BeginQueryOperationResult(const FString&,const FString&,const FGuid& Id,FGamePlatformEquipmentMutationCompletion C) override{LastOperation=Id;Mutation=MoveTemp(C);return true;}
};
class FManualEquipmentResolver final : public IGamePlatformEquipmentGameplayAssetResolver
{
public:
    bool ResolveAbilitySet(FName Id,TArray<TSubclassOf<UGameplayAbility>>& Out,TArray<TSubclassOf<UGameplayEffect>>&) override
    {if(Id!=TEXT("ManualNativeAbilitySet"))return false;Out.Add(UGameplayAbility::StaticClass());return true;}
    bool ResolveGameplayEffect(FName,TSubclassOf<UGameplayEffect>&) override{return false;}
};
#endif
struct FManualEquipmentView
{
    FDivineBeastsDemoRunToken Token;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UGamePlatformEquipmentComponent> Equipment;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;bool bAlive=true;uint64 Events=0;
    TWeakObjectPtr<UGameInstance> GI;TWeakObjectPtr<UGamePlatformEquipmentVisualComponent> Visual;TWeakObjectPtr<USkeletalMeshComponent> Avatar;
    FGamePlatformDataLease VisualLease;FDivineBeastsDemoOperationCompletion Pending;uint64 LoadGeneration=0;
    void Changed(){if(!bAlive || !World.IsValid() || World->bIsTearingDown || !Equipment.IsValid())return;++Events;
        if(Publish)Publish(Token,FString::Printf(TEXT("OwnerRevision=%lld PublicRevision=%lld PublicSlots=%d Events=%llu；本地夹具不证明网络复制"),
            Equipment->GetOwnerSnapshot().EquipmentRevision,Equipment->GetPublicSnapshot().PublicStateRevision,Equipment->GetPublicSnapshot().Slots.Num(),Events));}
};
class FManualEquipmentScenario final : public FPlatformManualBase
{
public:
    FManualEquipmentScenario(UWorld& W,FName F):FPlatformManualBase(W,F),View(MakeShared<FManualEquipmentView>()){}
    ~FManualEquipmentScenario() override{CleanupService();}
private:
    FGamePlatformResult PrepareService() override
    {
#if !WITH_EDITOR
        if(Feature!=TEXT("GamePlatformEquipment.04"))return ManualError(TEXT("Demo.EquipmentEditorAuthorityBridgeRequired"),TEXT("权威装备/GAS人工夹具需要Editor专用桥；普通Client不链接Server模块。"));
#endif
        if(SourceWorld->GetNetMode()==NM_Client)
            return ManualError(TEXT("Demo.EquipmentAuthorityFixtureRequired"),TEXT("本地自有装备夹具需要Standalone/PIE权威World；真实Client复制验收需开发服务器。"));
        FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
        Actor=SourceWorld->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Spawn);
        if(!Actor.IsValid())return ManualError(TEXT("Demo.EquipmentActorSpawn"),TEXT("自有装备夹具Actor创建失败。"));
        State=NewObject<UGamePlatformEquipmentComponent>(Actor.Get());Actor->AddInstanceComponent(State.Get());State->RegisterComponent();
        Visual=NewObject<UGamePlatformEquipmentVisualComponent>(Actor.Get());Actor->AddInstanceComponent(Visual.Get());Visual->RegisterComponent();
        Mesh=NewObject<USkeletalMeshComponent>(Actor.Get());Actor->AddInstanceComponent(Mesh.Get());Actor->SetRootComponent(Mesh.Get());Mesh->RegisterComponent();
        Actor->SetActorLocation(FVector(200,0,0));
        View->Token=Context.Token;View->World=SourceWorld;View->GI=Instance();View->Visual=Visual;View->Avatar=Mesh;View->Publish=Context.PublishObservation;View->Equipment=State;
        TWeakPtr<FManualEquipmentView> Weak(View);Changed=State->OnPublicEquipmentChanged.AddLambda([Weak](){if(auto V=Weak.Pin())V->Changed();});
#if WITH_EDITOR
        if(Feature!=TEXT("GamePlatformEquipment.04"))
        {
            Server=NewObject<UGamePlatformEquipmentServerComponent>(Actor.Get());Actor->AddInstanceComponent(Server.Get());Server->RegisterComponent();
            ASC=NewObject<UAbilitySystemComponent>(Actor.Get());Actor->AddInstanceComponent(ASC.Get());ASC->RegisterComponent();ASC->InitAbilityActorInfo(Actor.Get(),Actor.Get());
            Persistence=MakeShared<FManualEquipmentPort,ESPMode::ThreadSafe>();
            Grants=MakeShared<FGamePlatformEquipmentGASGrantPort,ESPMode::ThreadSafe>(MakeShared<FManualEquipmentResolver,ESPMode::ThreadSafe>());
        }
#endif
        View->Changed();return ManualObserved(TEXT("自有真实Native装备组件已准备；未写正式账号或复制源。"));
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;++View->LoadGeneration;
        auto Pending=MoveTemp(View->Pending);
        FGamePlatformResult Cleanup=FGamePlatformResult::Success();
        if(Visual.IsValid()){Visual->BindEquipmentComponent(nullptr);Visual->BindAvatar(nullptr,0);}
#if WITH_EDITOR
        if(Server.IsValid())Server->DestroyComponent();
#endif
        if(View->VisualLease.IsValid())
        {
            auto* Data=View->GI.IsValid()?IGamePlatformDataService::Get(*View->GI):nullptr;
            Cleanup=Data?Data->ReleaseResources(View->VisualLease):ManualError(TEXT("Demo.EquipmentVisualDataCleanup"),TEXT("Data服务失效；保留完整视觉租约诊断。"));
            if(Cleanup.IsSuccess())View->VisualLease={};
        }
        if(Pending)Pending(Context.Token,FGamePlatformResult::Cancelled(TEXT("自有装备视觉加载取消。")));
        if(State.IsValid())State->OnPublicEquipmentChanged.Remove(Changed);
        if(Visual.IsValid()){Visual->BindEquipmentComponent(nullptr);Visual->BindAvatar(nullptr,0);}
        if(Actor.IsValid()){if(!Actor->Destroy())return ManualError(TEXT("Demo.EquipmentActorCleanupFailed"),TEXT("自有Actor未能销毁，保留弱身份供重试。"));Actor.Reset();}
        State.Reset();Visual.Reset();Mesh.Reset();
#if WITH_EDITOR
        if(Persistence){Persistence->Load={};Persistence->Mutation={};}Server.Reset();ASC.Reset();Persistence.Reset();Grants.Reset();
#endif
        return Cleanup;
    }
    FGamePlatformEquipmentSnapshot Snapshot(const FManualArguments& A) const
    {
        FGamePlatformEquipmentSnapshot S;S.CharacterId=TEXT("ManualCharacter");S.EquipmentRevision=A.Int(TEXT("Revision"));
        if(A.Bool(TEXT("Equipped")))
        {
            FGamePlatformEquipmentSlotState X;X.SlotId=A.Id(TEXT("Slot"));X.ItemInstanceId=TEXT("ManualItem");X.ItemDefinitionId=TEXT("ManualItemDefinition");
            X.EquipmentDefinitionId=TEXT("ManualEquipment");X.VisualDefinitionId=A.Id(TEXT("VisualDefinition"));X.Revision=S.EquipmentRevision;S.Slots.Add(X);
        }
        return S;
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        FGamePlatformResult R;
        if(Op==TEXT("AcquireVisualDefinition"))
        {
            auto* Data=Instance()?IGamePlatformDataService::Get(*Instance()):nullptr;
            auto* Caller=SourceWorld->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
            if(!Data || !Caller || View->VisualLease.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.EquipmentVisualDataOwnership"),TEXT("需要真实Data/World调用者且没有旧视觉租约。")));return;}
            const auto Path=A.Asset(TEXT("VisualAsset"));const auto AvatarPath=A.Asset(TEXT("AvatarMesh"));const auto Socket=A.Id(TEXT("SocketName"));View->Pending=MoveTemp(Done);const uint64 G=++View->LoadGeneration;TWeakPtr<FManualEquipmentView> Weak(View);
            FGamePlatformResult Accepted;
            View->VisualLease=Data->AcquireResources({Path,AvatarPath},EGamePlatformDataLifetime::World,Caller,[Weak,G,Path,AvatarPath,Socket](const auto& L,const auto& Result)
            {
                auto V=Weak.Pin();if(!V || !V->bAlive || V->LoadGeneration!=G || !V->World.IsValid() || V->World->bIsTearingDown)return;
                const auto& Expected=V->VisualLease;
                const bool Match=L.ScopeId==Expected.ScopeId && L.LeaseId==Expected.LeaseId && L.Generation==Expected.Generation && L.IssuerProof==Expected.IssuerProof &&
                    L.DefinitionId==Expected.DefinitionId && L.Bundles==Expected.Bundles && L.ResourcePaths==Expected.ResourcePaths;
                auto R=Result;
                if(!Match)R=ManualError(TEXT("Demo.EquipmentVisualLeaseMismatch"),TEXT("完整视觉租约不匹配。"));
                else if(R.IsSuccess())
                {
                    auto* Definition=Cast<UGamePlatformEquipmentVisualDefinition>(Path.ResolveObject());
                    auto* AvatarAsset=Cast<USkeletalMesh>(AvatarPath.ResolveObject());
                    if(!Definition || !AvatarAsset || !V->Visual.IsValid() || !V->Avatar.IsValid())R=ManualError(TEXT("Demo.EquipmentVisualDefinitionType"),TEXT("实际Data对象须为外观定义与SkeletalMesh，组件须仍有效。"));
                    else
                    {
                        V->Avatar->SetSkeletalMeshAsset(AvatarAsset);
                        if(Socket.IsNone() || !V->Avatar->DoesSocketExist(Socket) || Definition->SocketName!=Socket)
                            R=ManualError(TEXT("Demo.EquipmentAvatarSocket"),TEXT("Avatar实际Socket/骨骼须存在且与外观定义一致；拒绝空Avatar。"));
                        else {V->Visual->RegisterVisualDefinition(Definition);R=ManualObserved(TEXT("真实Data加载外观定义和Avatar，已核对实际命名Socket/骨骼；可BindVisual并发布自有快照观察附件。"));}
                    }
                }
                V->Changed();auto C=MoveTemp(V->Pending);if(C)C(V->Token,R);
            },Accepted);
            if(!Accepted.IsSuccess()){auto C=MoveTemp(View->Pending);if(C)C(Context.Token,Accepted);}
            return;
        }
        if(Op==TEXT("PublishFixtureSnapshot"))
        {
            const auto S=Snapshot(A);FString Why;
            if(!ValidateGamePlatformEquipmentSnapshot(S,Why))R=FGamePlatformResult::Failure(TEXT("Demo.InvalidEquipmentFixture"),Why);
            else {State->SetServerSnapshots(S,static_cast<int32>(A.Int(TEXT("AvatarGeneration"))));R=ManualObserved(TEXT("真实Native组件接纳本地隔离快照；未发起后端装备。"));}
        }
        else if(Op==TEXT("BindVisual"))
        {
            if(!Mesh.IsValid() || !Mesh->GetSkeletalMeshAsset() || !Mesh->DoesSocketExist(A.Id(TEXT("SocketName"))))
            {Done(Context.Token,ManualError(TEXT("Demo.EquipmentAvatarNotReady"),TEXT("先成功AcquireVisualDefinition，拒绝绑定空Avatar或缺失Socket。")));return;}
            Visual->BindEquipmentComponent(State.Get());Visual->BindAvatar(Mesh.Get(),static_cast<int32>(A.Int(TEXT("AvatarGeneration"))));R=ManualObserved(TEXT("真实视觉组件绑定已加载Avatar/Socket与自有装备状态；附件由唯一正式视觉组件创建。"));
        }
        else if(Op==TEXT("RefreshVisual")){Visual->RefreshVisuals();R=ManualObserved(TEXT("真实视觉刷新已调用；没有外观定义不伪造网格成功。"));}
        else if(Op==TEXT("UnbindVisual")){Visual->BindEquipmentComponent(nullptr);Visual->BindAvatar(nullptr,0);R=FGamePlatformResult::Success();}
#if WITH_EDITOR
        else if(Server.IsValid())
        {
            if(Op==TEXT("NewOperation"))
            {
                if(Persistence->Load || Persistence->Mutation)R=ManualError(TEXT("Demo.EquipmentOperationStillPending"),TEXT("原装备请求仍在途/未知，不能换新身份。"));
                else {OperationId=FGuid::NewGuid();R=ManualObserved(TEXT("已在确定终态后生成新装备操作身份。"));}
            }
            else if(Op==TEXT("RegisterDefinition"))
            {
                Definition=NewObject<UGamePlatformEquipmentDefinition>(Actor.Get());Definition->EquipmentDefinitionId=TEXT("ManualEquipment");Definition->CompatibleItemDefinitionId=TEXT("ManualItemDefinition");
                Definition->AllowedSlotIds={A.Id(TEXT("Slot"))};Definition->AbilitySetDefinitionIds={TEXT("ManualNativeAbilitySet")};Server->RegisterDefinition(Definition.Get());R=ManualObserved(TEXT("真实服务器组件登记隔离定义，能力集映射至真实Native UGameplayAbility，仅测试授予/撤销。"));
            }
            else if(Op==TEXT("InitializeRuntime"))
                R=Server->InitializeEquipmentRuntime(TEXT("ManualPlayer"),TEXT("ManualCharacter"),State.Get(),Persistence,Grants)?ManualObserved(TEXT("真实装备加载已受理；等待手控Port。")):ManualError(TEXT("Demo.EquipmentInitializeRejected"),TEXT("真实装备初始化拒绝。"));
            else if(Op==TEXT("DeliverLoad") || Op==TEXT("DeliverMutation"))
            {
                const auto Error=A.Bool(TEXT("ResponseSuccess"))?EGamePlatformEquipmentError::None:EGamePlatformEquipmentError::PersistenceOutcomeUnknown;
                if(Op==TEXT("DeliverLoad") && Persistence->Load){auto C=MoveTemp(Persistence->Load);C(Snapshot(A),Error);R=ManualObserved(TEXT("原生装备加载响应已交付。"));}
                else if(Op==TEXT("DeliverMutation") && Persistence->Mutation){auto C=MoveTemp(Persistence->Mutation);C(Snapshot(A),Error);R=ManualObserved(TEXT("原生装备变更响应已交付；Port不计算装备事务。"));}
                else R=ManualError(TEXT("Demo.EquipmentNoPendingResponse"),TEXT("当前没有匹配响应。"));
            }
            else if(Op==TEXT("BindAvatar"))R=Server->BindAvatar(ASC.Get(),static_cast<int32>(A.Int(TEXT("AvatarGeneration"))))?FGamePlatformResult::Success():ManualError(TEXT("Demo.EquipmentBindAvatarRejected"),TEXT("真实Avatar应用失败或尚未就绪。"));
            else if(Op==TEXT("UnbindAvatar")){Server->BindAvatar(nullptr,static_cast<int32>(A.Int(TEXT("AvatarGeneration"))));R=ManualObserved(TEXT("真实Avatar解绑；本组件授予须撤销。"));}
            else if(Op==TEXT("Equip"))
            {
                FGamePlatformEquipRequest Q;Q.OperationId=OperationId;Q.CharacterId=TEXT("ManualCharacter");Q.SlotId=A.Id(TEXT("Slot"));Q.ItemInstanceId=TEXT("ManualItem");
                Q.ExpectedEquipmentRevision=A.Int(TEXT("Revision"));Q.ExpectedInventoryRevision=A.Int(TEXT("InventoryRevision"));
                const auto E=Server->RequestEquip(Q);R=E==EGamePlatformEquipmentError::None?ManualObserved(TEXT("真实Equip已受理；持久响应尚未交付。")):ManualError(TEXT("Demo.EquipmentEquipRejected"),TEXT("真实Equip预检拒绝；查看LastError。"));
            }
            else if(Op==TEXT("Unequip"))
            {
                FGamePlatformUnequipRequest Q;Q.OperationId=OperationId;Q.CharacterId=TEXT("ManualCharacter");Q.SlotId=A.Id(TEXT("Slot"));Q.ExpectedEquipmentRevision=A.Int(TEXT("Revision"));
                const auto E=Server->RequestUnequip(Q);R=E==EGamePlatformEquipmentError::None?ManualObserved(TEXT("真实Unequip已受理。")):ManualError(TEXT("Demo.EquipmentUnequipRejected"),TEXT("真实Unequip拒绝。"));
            }
            else if(Op==TEXT("Reconcile"))R=Server->Reconcile()?ManualObserved(TEXT("原生对账请求已受理；保留原操作身份。")):ManualError(TEXT("Demo.EquipmentReconcileRejected"),TEXT("真实Reconcile拒绝。"));
            else if(Op==TEXT("Inspect"))R=FGamePlatformResult::Success();
            else R=ManualError(TEXT("Demo.EquipmentOperation"),TEXT("装备操作不在白名单。"));
            if(!Alive() || !Server.IsValid() || !ASC.IsValid()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("装备事件中本案例已停止。")));return;}
            R.Message+=FString::Printf(TEXT("；Ready=%d LastError=%d GASAbilities=%d"),Server->IsEquipmentGameplayReady(),int32(Server->GetLastError()),ASC->GetActivatableAbilities().Num());
        }
#endif
        else if(Op==TEXT("Inspect"))R=FGamePlatformResult::Success();
        else R=ManualError(TEXT("Demo.EquipmentHostOperation"),TEXT("此操作需要Editor服务器桥/真实内容宿主。"));
        Done(Context.Token,R);
    }
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<UGamePlatformEquipmentComponent> State;
    TWeakObjectPtr<UGamePlatformEquipmentVisualComponent> Visual;
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    TWeakObjectPtr<UGamePlatformEquipmentDefinition> Definition;
    TSharedRef<FManualEquipmentView> View;
    FDelegateHandle Changed;
    FGuid OperationId=FGuid::NewGuid();
#if WITH_EDITOR
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> Server;
    TWeakObjectPtr<UAbilitySystemComponent> ASC;
    TSharedPtr<FManualEquipmentPort,ESPMode::ThreadSafe> Persistence;
    TSharedPtr<FGamePlatformEquipmentGASGrantPort,ESPMode::ThreadSafe> Grants;
#endif
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformEquipmentScenario(UWorld& W,FName F)
{return MakeUnique<FManualEquipmentScenario>(W,F);}
}

