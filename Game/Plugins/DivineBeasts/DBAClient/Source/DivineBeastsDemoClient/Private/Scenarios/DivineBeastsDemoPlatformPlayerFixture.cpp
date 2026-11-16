// 五个玩家服务的隔离人工Port夹具；只有Transport响应由操作者指定，规则/派生/对账均调用真实插件。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Scenarios/DivineBeastsDemoPlatformManualPorts.h"
#include "Services/GamePlatformInventoryClientSubsystem.h"
#include "Services/GamePlatformEntitlementClientSubsystem.h"
#include "Services/GamePlatformProgressionClientSubsystem.h"
#include "Services/GamePlatformLiveOpsClientSubsystem.h"
#include "Services/GamePlatformCommerceClientSubsystem.h"
#include "ViewModels/GamePlatformCommerceViewModel.h"
#include "Definitions/GamePlatformProgressionTrackDefinition.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo
{
/** 强引用只保活自己构造的真实Native对象；LocalPlayer无Viewport，不绑定正式Online账号。 */
struct FManualPlayerState : TSharedFromThis<FManualPlayerState>
{
    FDivineBeastsDemoRunToken Token;
    TWeakObjectPtr<UWorld> World;
    TStrongObjectPtr<ULocalPlayer> Player;
    TStrongObjectPtr<UGamePlatformInventoryClientSubsystem> Inventory;
    TStrongObjectPtr<UGamePlatformEntitlementClientSubsystem> Entitlement;
    TStrongObjectPtr<UGamePlatformProgressionClientSubsystem> Progression;
    TStrongObjectPtr<UGamePlatformLiveOpsClientSubsystem> LiveOps;
    TStrongObjectPtr<UGamePlatformCommerceClientSubsystem> Commerce;
    TStrongObjectPtr<UGamePlatformProgressionTrackDefinition> Curve;
    TSharedPtr<FManualInventoryPort,ESPMode::ThreadSafe> InventoryPort;
    TSharedPtr<FManualEntitlementPort,ESPMode::ThreadSafe> EntitlementPort;
    TSharedPtr<FManualProgressionPort,ESPMode::ThreadSafe> ProgressionPort;
    TSharedPtr<FManualLiveOpsPort,ESPMode::ThreadSafe> LiveOpsPort;
    TSharedPtr<FManualCommercePort,ESPMode::ThreadSafe> CommercePort;

    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    FGuid OperationId=FGuid::NewGuid();
    uint64 EventCount=0;
    bool bAlive=true;

    FString LastOrderState;
    FString Projection() const
    {
        FString P=TEXT("隔离Port夹具；不代表联网、支付或权威事务验收；");
        if(Inventory.IsValid()) P+=FString::Printf(TEXT("Inventory State=%d Error=%d Rev=%lld Items=%d Pending=%s"),
            int32(Inventory->GetState()),int32(Inventory->GetLastError()),Inventory->GetInventoryRevision(),Inventory->GetViewModelsView().Num(),*Inventory->GetPendingOperationId().ToString());
        if(Entitlement.IsValid()) P+=FString::Printf(TEXT("Entitlement State=%d Error=%d Rev=%lld Views=%d"),
            int32(Entitlement->GetState()),int32(Entitlement->GetLastError()),Entitlement->GetSnapshotRevision(),Entitlement->GetViewModelsView().Num());
        if(Progression.IsValid()) P+=FString::Printf(TEXT("Progression State=%d Error=%d Rev=%lld Views=%d"),
            int32(Progression->GetState()),int32(Progression->GetLastError()),Progression->GetProgressionRevision(),Progression->GetViewModelsView().Num());
        if(LiveOps.IsValid()) P+=FString::Printf(TEXT("LiveOps State=%d Error=%d Catalog=%lld Player=%lld Seasons=%d Events=%d SignIns=%d"),
            int32(LiveOps->GetState()),int32(LiveOps->GetLastError()),LiveOps->GetCatalogRevision(),LiveOps->GetPlayerStateRevision(),
            LiveOps->GetActiveSeasonIds().Num(),LiveOps->GetEventViewModels().Num(),LiveOps->GetSignInViewModels().Num());
        if(Commerce.IsValid())
        {
            const auto* V=Commerce->GetCommerceViewModel();
            P+=FString::Printf(TEXT("Commerce State=%d Catalog=%lld Products=%d Offers=%d Order=%s"),
                int32(Commerce->GetState()),Commerce->GetCatalogRevision(),V?V->GetProductsView().Num():0,V?V->GetOffersView().Num():0,*LastOrderState);
        }
        if(InventoryPort)P+=InventoryPort->PendingText();
        if(EntitlementPort)P+=EntitlementPort->PendingText();
        if(ProgressionPort)P+=ProgressionPort->PendingText();
        if(LiveOpsPort)P+=LiveOpsPort->PendingText();
        if(CommercePort)P+=CommercePort->PendingText();
        return P+FString::Printf(TEXT("；真实事件=%llu"),EventCount);
    }
    void Changed()
    {
        if(!bAlive || !World.IsValid() || World->bIsTearingDown) return;
        ++EventCount;if(Publish)Publish(Token,Projection());

    }
    void Close()
    {
        if(!bAlive) return;
        bAlive=false;
        // 先切断自己的UI回调，再由真实Deinitialize撤销Port、全局事件及领域请求。
        if(Inventory.IsValid())Inventory->Deinitialize();
        if(Entitlement.IsValid())Entitlement->Deinitialize();
        if(Progression.IsValid())Progression->Deinitialize();
        if(LiveOps.IsValid())LiveOps->Deinitialize();
        if(Commerce.IsValid())Commerce->Deinitialize();

        Inventory.Reset();Entitlement.Reset();Progression.Reset();LiveOps.Reset();Commerce.Reset();Curve.Reset();Player.Reset();

    }
};
/** 交付回调只有显式手动动作消费一次；Port不实现物品、支付、权益或成长业务规则。 */
template<typename TCallback,typename TResponse,typename TError>
bool DeliverManualResponse(TManualPortPending<TCallback>& Pending,TResponse Response,TError Error,const FManualArguments& A)
{
    FManualPortIdentity Identity;auto Local=Pending.Take(A.Id(TEXT("ResponseKind")),A.Int(TEXT("ResponseSequence")),Identity);
    if(!Local)return false;
    Local(MoveTemp(Response),Error);return true; // 完成的是交付动作，不是业务终态。
}
template<typename TCallback> bool PendingMatches(const TManualPortPending<TCallback>& Pending,const FManualArguments& A)
{return Pending.Matches(A.Id(TEXT("ResponseKind")),A.Int(TEXT("ResponseSequence")));}

class FPlatformPlayerFixture final : public FPlatformManualBase
{
public:
    FPlatformPlayerFixture(UWorld& W,FName F):FPlatformManualBase(W,F),S(MakeShared<FManualPlayerState>()){}
    ~FPlatformPlayerFixture() override {S->Close();}
private:
    FGamePlatformResult PrepareService() override
    {
        if(!GEngine)return ManualError(TEXT("Demo.EngineHostMissing"),TEXT("隔离LocalPlayer必须有真实Engine Within宿主。"));
        S->Token=Context.Token;S->World=SourceWorld;S->Publish=Context.PublishObservation;S->Player.Reset(NewObject<ULocalPlayer>(GEngine));
        TWeakPtr<FManualPlayerState> Weak(S);
        auto Changed=[Weak](){if(auto P=Weak.Pin())P->Changed();};
        FSubsystemCollection<ULocalPlayerSubsystem> InitializationContext;
        const FString F=Feature.ToString();
        if(F.StartsWith(TEXT("GamePlatformInventory.")))
        {
            S->Inventory.Reset(NewObject<UGamePlatformInventoryClientSubsystem>(S->Player.Get()));
            S->Inventory->Initialize(InitializationContext);S->InventoryPort=MakeShared<FManualInventoryPort,ESPMode::ThreadSafe>();
            S->Inventory->OnChanged.AddLambda(Changed);
        }
        else if(F==TEXT("GamePlatformEntitlement.01"))
        {
            S->Entitlement.Reset(NewObject<UGamePlatformEntitlementClientSubsystem>(S->Player.Get()));
            S->Entitlement->Initialize(InitializationContext);S->EntitlementPort=MakeShared<FManualEntitlementPort,ESPMode::ThreadSafe>();
            S->Entitlement->OnChanged.AddLambda(Changed);
        }
        else if(F==TEXT("GamePlatformProgression.02"))
        {
            S->Progression.Reset(NewObject<UGamePlatformProgressionClientSubsystem>(S->Player.Get()));
            S->Progression->Initialize(InitializationContext);S->ProgressionPort=MakeShared<FManualProgressionPort,ESPMode::ThreadSafe>();
            S->Progression->OnViewChanged.AddLambda(Changed);
        }
        else if(F.StartsWith(TEXT("GamePlatformLiveOps.")))
        {
            S->LiveOps.Reset(NewObject<UGamePlatformLiveOpsClientSubsystem>(S->Player.Get()));
            S->LiveOps->Initialize(InitializationContext);S->LiveOpsPort=MakeShared<FManualLiveOpsPort,ESPMode::ThreadSafe>();
            S->LiveOps->OnViewChanged.AddLambda(Changed);
            S->LiveOps->OnClaimChanged.AddLambda([Weak](auto){if(auto P=Weak.Pin())P->Changed();});
        }
        else if(F.StartsWith(TEXT("GamePlatformCommerceUI.")))
        {
            S->Commerce.Reset(NewObject<UGamePlatformCommerceClientSubsystem>(S->Player.Get()));
            S->Commerce->Initialize(InitializationContext);S->CommercePort=MakeShared<FManualCommercePort,ESPMode::ThreadSafe>();
            S->Commerce->OnCommerceStateChanged.AddLambda(Changed);
            S->Commerce->OnOrderChanged.AddLambda([Weak](auto Order){if(auto P=Weak.Pin()){P->LastOrderState=Order.OrderState;P->Changed();}});
        }
        else return ManualError(TEXT("Demo.PlayerFixtureFeature"),TEXT("功能不属于封闭玩家夹具。"));
        S->Changed();return ManualObserved(S->Projection());
    }
    FGamePlatformResult CleanupService() override {S->Close();return FGamePlatformResult::Success();}
    FGamePlatformInventorySnapshot InventoryResponse(const FManualArguments& A) const
    {
        FGamePlatformInventorySnapshot R;R.InventoryRevision=A.Int(TEXT("Revision"));
        FGamePlatformInventoryContainerSnapshot C;C.ContainerId=TEXT("main");C.Capacity=static_cast<int32>(A.Int(TEXT("Capacity")));R.Containers.Add(C);
        for(int32 I=0;I<2;++I)
        {
            if((I==0 && !A.Bool(TEXT("IncludeSource"))) || (I==1 && !A.Bool(TEXT("IncludeOther"))))continue;
            FGamePlatformInventoryItemInstance X;X.ItemInstanceId=I?A.Id(TEXT("Other")).ToString():A.Id(TEXT("Subject")).ToString();
            X.ItemDefinitionId=TEXT("ManualConsumable");X.Quantity=static_cast<int32>(A.Int(I?TEXT("OtherQuantity"):TEXT("SourceQuantity")));X.SlotIndex=static_cast<int32>(A.Int(I?TEXT("ItemBSlot"):TEXT("ItemASlot")));X.Revision=R.InventoryRevision;X.MaxStackSize=100;
            R.Items.Add(X);
        }
        if(A.Bool(TEXT("QuickbarAssigned"))){FGamePlatformInventoryQuickbarSlot Q;Q.SlotIndex=static_cast<int32>(A.Int(TEXT("Slot")));Q.ItemInstanceId=A.Id(TEXT("Subject")).ToString();Q.Revision=R.InventoryRevision;R.Quickbar.Add(Q);}
        return R;
    }
    /** Begin即时报告受理；Delivery即时报告已交付。真实领域事件只投影，不能消费命令完成端。 */
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        if(Op==TEXT("Inspect")){Done(Context.Token,ManualObserved(S->Projection()));return;}
        if(Op==TEXT("ResetAccount"))
        {
            if(S->Inventory.IsValid())S->Inventory->ResetAccount();
            if(S->Entitlement.IsValid())S->Entitlement->ResetAccount();
            if(S->Progression.IsValid())S->Progression->ResetAccount();
            if(S->LiveOps.IsValid())S->LiveOps->ResetAccount();
            if(S->Commerce.IsValid())S->Commerce->ResetAccount();
            Done(Context.Token,ManualObserved(S->Projection()));return;
        }
        bool Accepted=false;
        if(Op==TEXT("ConfigureAccount"))
        {
            const FString Account=TEXT("ManualFixture_")+Context.Token.RunId.ToString(EGuidFormats::Digits)+A.Id(TEXT("Account")).ToString();
            if(S->Inventory.IsValid())Accepted=S->Inventory->ConfigureAuthenticatedAccount(Account,S->InventoryPort);
            if(S->Entitlement.IsValid())Accepted=S->Entitlement->ConfigureAuthenticatedAccount(Account,S->EntitlementPort);
            if(S->Progression.IsValid())Accepted=S->Progression->ConfigureAuthenticatedAccount(Account,S->ProgressionPort);
            if(S->LiveOps.IsValid())Accepted=S->LiveOps->ConfigureAuthenticatedAccount(Account,S->LiveOpsPort);
            if(S->Commerce.IsValid())Accepted=S->Commerce->ConfigureAuthenticatedAccount(Account,S->CommercePort);
        }
        else if(Op==TEXT("Refresh"))
        {
            if(S->Inventory.IsValid())Accepted=S->Inventory->RefreshSnapshot();
            if(S->Entitlement.IsValid())Accepted=S->Entitlement->RefreshSnapshot();
            if(S->Progression.IsValid())Accepted=S->Progression->RefreshSnapshot();
            if(S->LiveOps.IsValid())Accepted=S->LiveOps->RefreshAll();
            if(S->Commerce.IsValid())Accepted=S->Commerce->RefreshCatalog();
        }
        else if(Op==TEXT("RegisterCurve") && S->Progression.IsValid())
        {
            S->Curve.Reset(NewObject<UGamePlatformProgressionTrackDefinition>());
            S->Curve->ProgressionTrackId=A.Id(TEXT("Subject"));S->Curve->MaxLevel=3;S->Curve->CumulativeXPThresholds={0,100,300};
            S->Curve->CurveVersion=static_cast<int32>(A.Int(TEXT("CurveVersion")));FString Why;if(!S->Curve->Validate(Why)){Done(Context.Token,FGamePlatformResult::Failure(TEXT("Demo.InvalidFixtureCurve"),Why));return;}
            S->Progression->RegisterTrackDefinition(S->Curve.Get());Accepted=true;
        }
        else if(S->Inventory.IsValid())
        {
            auto* C=S->Inventory.Get();const FString Id=A.Id(TEXT("Subject")).ToString();FGuid Requested;
            if(Op==TEXT("Move"))Requested=C->RequestMove(Id,TEXT("main"),static_cast<int32>(A.Int(TEXT("Slot"))));
            else if(Op==TEXT("Split"))Requested=C->RequestSplit(Id,static_cast<int32>(A.Int(TEXT("Quantity"))),TEXT("main"),static_cast<int32>(A.Int(TEXT("Slot"))));
            else if(Op==TEXT("Merge"))Requested=C->RequestMerge(Id,A.Id(TEXT("Other")).ToString());
            else if(Op==TEXT("SetQuickbar"))Requested=C->RequestSetQuickbar(static_cast<int32>(A.Int(TEXT("Slot"))),Id);
            else if(Op==TEXT("ClearQuickbar"))Requested=C->RequestClearQuickbar(static_cast<int32>(A.Int(TEXT("Slot"))));
            else if(Op==TEXT("RetryPending")){Accepted=C->RetryPendingOperation();goto CompleteBegin;}
            else if(Op==TEXT("ReadPage"))
            {
                const auto Page=C->GetContainerSlotPage(TEXT("main"),static_cast<int32>(A.Int(TEXT("Slot"))),8);
                Done(Context.Token,ManualObserved(S->Projection()+FString::Printf(TEXT("；Page=%d Quickbar=%d"),Page.Num(),C->GetQuickbarSlots().Num())));return;
            }
            else goto Deliver;
            Accepted=Requested.IsValid();if(Accepted)S->OperationId=Requested;
        }
        else if(S->LiveOps.IsValid())
        {
            if(Op==TEXT("RefreshPlayer"))Accepted=S->LiveOps->RefreshPlayerState();
            else if(Op==TEXT("Claim"))Accepted=S->LiveOps->ClaimSignIn(A.Id(TEXT("Subject")),S->OperationId);
            else if(Op==TEXT("Reconcile"))Accepted=S->LiveOps->ReconcileClaim(S->OperationId);
            else goto Deliver;
        }
        else if(S->Commerce.IsValid())
        {
            if(Op==TEXT("CreateIntent"))Accepted=S->Commerce->CreatePurchaseIntent(A.Id(TEXT("Subject")),static_cast<int32>(A.Int(TEXT("Quantity"))),S->OperationId);
            else if(Op==TEXT("Purchase"))Accepted=S->Commerce->BeginPurchase(TEXT("ManualIntent"));
            else if(Op==TEXT("SubmitFixtureReceipt"))Accepted=S->Commerce->SubmitReceipt(TEXT("ManualOrder"),TEXT("MANUAL_FIXTURE_NOT_A_PAYMENT_CREDENTIAL"));
            else if(Op==TEXT("RefreshOrder"))Accepted=S->Commerce->RefreshOrder(TEXT("ManualOrder"));
            else if(Op==TEXT("Reconcile"))Accepted=S->Commerce->ReconcileOrder(TEXT("ManualOrder"));
            else goto Deliver;
        }
        else goto Deliver;
CompleteBegin:
        Done(Context.Token,Accepted?ManualObserved(TEXT("真实公开命令已受理；等待手动Port响应，未宣称Ready或事务成功。")+S->Projection()):
            ManualError(TEXT("Demo.NativeCommandRejected"),TEXT("真实服务拒绝命令；检查账号、忙碌、参数及最近领域状态。")));return;
Deliver:
        // 各按钮与响应类型固定绑定；不能提交任意JSON/HTTP正文，也没有生产地址入口。
        const int64 Mode=A.Int(TEXT("ErrorMode"));
        if(Mode<0 || Mode>3){Done(Context.Token,ManualError(TEXT("Demo.PortErrorMode"),TEXT("ErrorMode必须为0正常/1失败/2结果未知/3查无原操作。")));return;}
        const FName SelectedKind=A.Id(TEXT("ResponseKind"));const int64 SelectedSequence=A.Int(TEXT("ResponseSequence"));
        bool Delivered=false;
        if(S->InventoryPort)
        {
            auto P=S->InventoryPort;
            const auto Error=Mode==0?EGamePlatformInventoryError::None:Mode==1?EGamePlatformInventoryError::BackendUnavailable:
                Mode==2?EGamePlatformInventoryError::OutcomeUnknown:EGamePlatformInventoryError::OperationNotFound;
            if(Op==TEXT("DeliverRead"))Delivered=DeliverManualResponse(P->BeginGetSnapshotCompletion,InventoryResponse(A),Error,A);
            else if(Op==TEXT("DeliverMutation"))
            {
                FGamePlatformInventoryMutationResult R;R.OperationId=S->OperationId;R.Snapshot=InventoryResponse(A);
                if(PendingMatches(P->BeginGetOperationCompletion,A))Delivered=DeliverManualResponse(P->BeginGetOperationCompletion,R,Error,A);
                else if(PendingMatches(P->BeginMoveCompletion,A))Delivered=DeliverManualResponse(P->BeginMoveCompletion,R,Error,A);
                else if(PendingMatches(P->BeginSplitCompletion,A))Delivered=DeliverManualResponse(P->BeginSplitCompletion,R,Error,A);
                else if(PendingMatches(P->BeginMergeCompletion,A))Delivered=DeliverManualResponse(P->BeginMergeCompletion,R,Error,A);
                else if(PendingMatches(P->BeginSetQuickbarCompletion,A))Delivered=DeliverManualResponse(P->BeginSetQuickbarCompletion,R,Error,A);
                else if(PendingMatches(P->BeginClearQuickbarCompletion,A))Delivered=DeliverManualResponse(P->BeginClearQuickbarCompletion,R,Error,A);
            }
        }
        if(S->EntitlementPort && Op==TEXT("DeliverRead"))
        {
            FGamePlatformEntitlementSnapshot R;R.Revision=A.Int(TEXT("Revision"));R.GeneratedAtUtc=FDateTime::UtcNow();
            FGamePlatformEntitlementEntry E;E.EntitlementId=A.Id(TEXT("Subject"));E.TargetId=A.Id(TEXT("Other"));E.TargetType=EGamePlatformEntitlementTargetType::Hero;
            E.Status=A.Bool(TEXT("Flag"))?EGamePlatformEntitlementStatus::Active:EGamePlatformEntitlementStatus::Revoked;R.Entitlements.Add(E);
            Delivered=DeliverManualResponse(S->EntitlementPort->BeginGetSnapshotCompletion,R,Mode==0?EGamePlatformEntitlementError::None:EGamePlatformEntitlementError::BackendUnavailable,A);
        }
        if(S->ProgressionPort && Op==TEXT("DeliverRead"))
        {
            FGamePlatformProgressionSnapshot R;R.ProgressionRevision=A.Int(TEXT("Revision"));R.GeneratedAtUtc=FDateTime::UtcNow();
            FGamePlatformProgressionTrackState T;T.SubjectType=EGamePlatformProgressionSubjectType::Character;T.SubjectId=A.Id(TEXT("Other")).ToString();
            T.ProgressionTrackId=A.Id(TEXT("Subject"));T.TotalXP=A.Int(TEXT("Quantity"));T.Level=static_cast<int32>(A.Int(TEXT("Level")));T.MaxLevel=3;
            T.CurveVersion=static_cast<int32>(A.Int(TEXT("CurveVersion")));T.Revision=R.ProgressionRevision;R.Tracks.Add(T);
            Delivered=DeliverManualResponse(S->ProgressionPort->BeginGetSnapshotCompletion,R,Mode==0?EGamePlatformProgressionError::None:EGamePlatformProgressionError::BackendUnavailable,A);
        }
        if(S->LiveOpsPort)
        {
            auto P=S->LiveOpsPort;
            const auto Error=Mode==0?EGamePlatformLiveOpsError::None:Mode==2?EGamePlatformLiveOpsError::OutcomeUnknown:EGamePlatformLiveOpsError::BackendUnavailable;
            if(Op==TEXT("DeliverRead"))
            {
                FGamePlatformLiveOpsCatalogSnapshot R;R.CatalogVersion=1;R.CatalogRevision=A.Int(TEXT("Revision"));R.PublishedAtUtc=FDateTime::UtcNow();R.ServerTimeUtc=R.PublishedAtUtc;
                FGamePlatformLiveOpsSignInCampaign C;C.CampaignId=A.Id(TEXT("Subject"));C.TimeWindow.StartsAtUtc=R.ServerTimeUtc-FTimespan::FromDays(1);C.RewardCount=1;R.SignInCampaigns.Add(C);
                Delivered=DeliverManualResponse(P->BeginGetCatalogCompletion,R,Error,A);
            }
            else if(Op==TEXT("DeliverPlayer"))
            {
                FGamePlatformLiveOpsPlayerState R;R.PlayerStateRevision=A.Int(TEXT("Revision"));R.GeneratedAtUtc=FDateTime::UtcNow();R.ServerTimeUtc=R.GeneratedAtUtc;
                FGamePlatformLiveOpsCampaignState C;C.CampaignId=A.Id(TEXT("Subject"));C.CurrentPeriodKey=TEXT("ManualPeriod");C.bClaimedCurrentPeriod=A.Bool(TEXT("Flag"));R.CampaignStates.Add(C);
                Delivered=DeliverManualResponse(P->BeginGetPlayerStateCompletion,R,Error,A);
            }
            else if(Op==TEXT("DeliverClaim"))
            {
                FGamePlatformLiveOpsClaimResult R;R.ClaimId=TEXT("ManualClaim");R.ClaimOperationId=S->OperationId.ToString(EGuidFormats::DigitsWithHyphens);
                R.CampaignId=A.Id(TEXT("Subject"));R.PeriodKey=TEXT("ManualPeriod");R.Status=A.Bool(TEXT("Flag"))?TEXT("fulfilled"):TEXT("pending");
                R.PlayerStateRevision=A.Int(TEXT("Revision"));R.ServerTimeUtc=FDateTime::UtcNow();
                if(PendingMatches(P->BeginQueryClaimOperationCompletion,A))Delivered=DeliverManualResponse(P->BeginQueryClaimOperationCompletion,R,Error,A);
                else Delivered=DeliverManualResponse(P->BeginClaimSignInCompletion,R,Error,A);
            }
        }
        if(S->CommercePort)
        {
            auto P=S->CommercePort;
            const auto Error=Mode==0?EGamePlatformCommerceError::None:Mode==2?EGamePlatformCommerceError::OutcomeUnknown:EGamePlatformCommerceError::BackendUnavailable;
            if(Op==TEXT("DeliverRead"))
            {
                FGamePlatformCommerceCatalogSnapshot R;R.CatalogRevision=A.Int(TEXT("Revision"));R.GeneratedAtUtc=FDateTime::UtcNow();R.ServerTimeUtc=R.GeneratedAtUtc;
                FGamePlatformCommerceProductCardView Product;Product.ProductId=TEXT("ManualProduct");Product.ProductType=TEXT("item");Product.NameKey=TEXT("人工Port产品");R.Products.Add(Product);
                FGamePlatformCommerceOfferView Offer;Offer.OfferId=A.Id(TEXT("Subject"));Offer.ProductId=Product.ProductId;Offer.OfferRevision=R.CatalogRevision;
                Offer.Price.PriceId=TEXT("ManualPrice");Offer.Price.CurrencyCode=TEXT("CNY");Offer.Price.UnitAmountMinor=A.Int(TEXT("AmountMinor"));
                Offer.StartsAtUtc=R.ServerTimeUtc-FTimespan::FromDays(1);Offer.bActiveProjection=true;R.Offers.Add(Offer);
                Delivered=DeliverManualResponse(P->BeginGetCatalogCompletion,R,Error,A);
            }
            else if(Op==TEXT("DeliverIntent"))
            {
                FGamePlatformCommercePurchaseIntentView R;R.PurchaseIntentId=TEXT("ManualIntent");R.RequestId=S->OperationId.ToString(EGuidFormats::DigitsWithHyphens);
                R.ProductId=TEXT("ManualProduct");R.OfferId=A.Id(TEXT("Subject"));R.Quantity=static_cast<int32>(A.Int(TEXT("Quantity")));R.CatalogRevision=A.Int(TEXT("Revision"));
                R.OfferRevision=R.CatalogRevision;R.ExpiresAtUtc=FDateTime::UtcNow()+FTimespan::FromMinutes(5);
                R.Price.CurrencyCode=TEXT("CNY");R.Price.UnitAmountMinor=A.Int(TEXT("AmountMinor"));R.TotalAmountMinor=A.Int(TEXT("AmountMinor"));
                Delivered=DeliverManualResponse(P->BeginCreatePurchaseIntentCompletion,R,Error,A);
            }
            else if(Op==TEXT("DeliverOrder"))
            {
                FGamePlatformCommerceOrderStatusView R;R.OrderId=TEXT("ManualOrder");R.PurchaseIntentId=TEXT("ManualIntent");R.ProductId=TEXT("ManualProduct");
                R.OfferId=A.Id(TEXT("Subject"));R.Quantity=static_cast<int32>(A.Int(TEXT("Quantity")));R.OrderRevision=A.Int(TEXT("Revision"));
                R.OrderState=A.Bool(TEXT("Flag"))?TEXT("fulfilled"):TEXT("pending");R.PaymentState=A.Bool(TEXT("Flag"))?TEXT("confirmed"):TEXT("pending");
                R.FulfillmentState=A.Bool(TEXT("Flag"))?TEXT("fulfilled"):TEXT("pending");
                if(PendingMatches(P->BeginPurchaseCompletion,A))Delivered=DeliverManualResponse(P->BeginPurchaseCompletion,R,Error,A);
                else if(PendingMatches(P->BeginSubmitReceiptCompletion,A))Delivered=DeliverManualResponse(P->BeginSubmitReceiptCompletion,R,Error,A);
                else if(PendingMatches(P->BeginGetOrderCompletion,A))Delivered=DeliverManualResponse(P->BeginGetOrderCompletion,R,Error,A);
                else if(PendingMatches(P->BeginReconcileOrderCompletion,A))Delivered=DeliverManualResponse(P->BeginReconcileOrderCompletion,R,Error,A);
            }
        }
        if(!S->bAlive || !Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("交付回调中本运行已停止。")));return;}
        Done(Context.Token,Delivered?ManualObserved(FString::Printf(TEXT("手控Port已交付 %s Sequence=%lld；这是交付终态，不是业务成功/Ready。原生状态可不变或继续等待。"),*SelectedKind.ToString(),SelectedSequence)+S->Projection()):
            ManualError(TEXT("Demo.PortResponseIdentityMismatch"),TEXT("没有精确匹配的请求类型/Sequence；未消费其它完成端。")));
        // Changed仅作只读投影，永不完成交付操作；无变化响应也不会占住UI。
    }
    TSharedRef<FManualPlayerState> S;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformPlayerFixture(UWorld& W,FName F)
{return MakeUnique<FPlatformPlayerFixture>(W,F);}
}

