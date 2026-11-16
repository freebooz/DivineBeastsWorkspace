// 手控Port只存请求完成端和不可混淆身份，不实施业务规则。所有交付先Take到局部，再调用。
#pragma once
#include "Interfaces/GamePlatformInventoryClientTransport.h"
#include "Interfaces/GamePlatformEntitlementClientTransport.h"
#include "Interfaces/GamePlatformProgressionClientTransport.h"
#include "Interfaces/GamePlatformLiveOpsClientTransport.h"
#include "Interfaces/GamePlatformCommerceClientTransport.h"
namespace DivineBeasts::Demo
{
struct FManualPortIdentity
{
    FGuid ScopeId,RequestId;
    uint64 Generation=0,Sequence=0;
    FName Kind;
    FString LogicalId;
    FString Text() const {return FString::Printf(TEXT("%s Seq=%llu Gen=%llu Scope=%s Request=%s Logical=%s"),
        *Kind.ToString(),Sequence,Generation,*ScopeId.ToString(),*RequestId.ToString(),*LogicalId);}
};
template<typename TCallback> class TManualPortPending
{
public:
    FManualPortIdentity Identity;
    explicit operator bool() const {return static_cast<bool>(Callback);}
    bool Store(FGuid Scope,uint64 Generation,uint64& Next,FName Kind,FString Logical,TCallback C)
    {
        if(Callback || !C || Next>=MAX_int32)return false;
        Identity={Scope,FGuid::NewGuid(),Generation,++Next,Kind,MoveTemp(Logical)};Callback=MoveTemp(C);return true;
    }
    bool Matches(FName Kind,int64 Sequence) const
    {return Callback && Sequence>0 && Identity.Kind==Kind && Identity.Sequence==static_cast<uint64>(Sequence);}
    TCallback Take(FName Kind,int64 Sequence,FManualPortIdentity& Out)
    {
        if(!Matches(Kind,Sequence))return {};
        Out=Identity;auto Local=MoveTemp(Callback);Identity={};return Local;
    }
    void Reset(){Callback={};Identity={};}
private:TCallback Callback;
};
class FManualInventoryPort final : public IGamePlatformInventoryClientTransport
{
public:
    const FGuid ScopeId=FGuid::NewGuid();uint64 Generation=1,NextSequence=0;
    TManualPortPending<FGamePlatformInventorySnapshotCompletion> BeginGetSnapshotCompletion;
    bool BeginGetSnapshot(FGamePlatformInventorySnapshotCompletion Completion) override {return BeginGetSnapshotCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetSnapshot"),TEXT("read"),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformInventoryMutationCompletion> BeginGetOperationCompletion;
    bool BeginGetOperation(const FGuid& OperationId, FGamePlatformInventoryMutationCompletion Completion) override {return BeginGetOperationCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetOperation"),OperationId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformInventoryMutationCompletion> BeginMoveCompletion;
    bool BeginMove(const FGamePlatformInventoryMoveRequest& Request, FGamePlatformInventoryMutationCompletion Completion) override {return BeginMoveCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginMove"),Request.OperationId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformInventoryMutationCompletion> BeginSplitCompletion;
    bool BeginSplit(const FGamePlatformInventorySplitRequest& Request, FGamePlatformInventoryMutationCompletion Completion) override {return BeginSplitCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginSplit"),Request.OperationId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformInventoryMutationCompletion> BeginMergeCompletion;
    bool BeginMerge(const FGamePlatformInventoryMergeRequest& Request, FGamePlatformInventoryMutationCompletion Completion) override {return BeginMergeCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginMerge"),Request.OperationId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformInventoryMutationCompletion> BeginSetQuickbarCompletion;
    bool BeginSetQuickbar(const FGamePlatformInventoryQuickbarRequest& Request, FGamePlatformInventoryMutationCompletion Completion) override {return BeginSetQuickbarCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginSetQuickbar"),Request.OperationId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformInventoryMutationCompletion> BeginClearQuickbarCompletion;
    bool BeginClearQuickbar(const FGamePlatformInventoryQuickbarRequest& Request, FGamePlatformInventoryMutationCompletion Completion) override {return BeginClearQuickbarCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginClearQuickbar"),Request.OperationId.ToString(),MoveTemp(Completion));}
    void CancelAllRequests() override {++Generation;BeginGetSnapshotCompletion.Reset();BeginGetOperationCompletion.Reset();BeginMoveCompletion.Reset();BeginSplitCompletion.Reset();BeginMergeCompletion.Reset();BeginSetQuickbarCompletion.Reset();BeginClearQuickbarCompletion.Reset();}
    FString PendingText() const {FString Out;if(BeginGetSnapshotCompletion)Out+=TEXT("；")+BeginGetSnapshotCompletion.Identity.Text();if(BeginGetOperationCompletion)Out+=TEXT("；")+BeginGetOperationCompletion.Identity.Text();if(BeginMoveCompletion)Out+=TEXT("；")+BeginMoveCompletion.Identity.Text();if(BeginSplitCompletion)Out+=TEXT("；")+BeginSplitCompletion.Identity.Text();if(BeginMergeCompletion)Out+=TEXT("；")+BeginMergeCompletion.Identity.Text();if(BeginSetQuickbarCompletion)Out+=TEXT("；")+BeginSetQuickbarCompletion.Identity.Text();if(BeginClearQuickbarCompletion)Out+=TEXT("；")+BeginClearQuickbarCompletion.Identity.Text();return Out;}
};
class FManualEntitlementPort final : public IGamePlatformEntitlementClientTransport
{
public:
    const FGuid ScopeId=FGuid::NewGuid();uint64 Generation=1,NextSequence=0;
    TManualPortPending<FGamePlatformEntitlementSnapshotCompletion> BeginGetSnapshotCompletion;
    bool BeginGetSnapshot(FGamePlatformEntitlementSnapshotCompletion Completion) override {return BeginGetSnapshotCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetSnapshot"),TEXT("read"),MoveTemp(Completion));}
    void CancelAllRequests() override {++Generation;BeginGetSnapshotCompletion.Reset();}
    FString PendingText() const {FString Out;if(BeginGetSnapshotCompletion)Out+=TEXT("；")+BeginGetSnapshotCompletion.Identity.Text();return Out;}
};
class FManualProgressionPort final : public IGamePlatformProgressionClientTransport
{
public:
    const FGuid ScopeId=FGuid::NewGuid();uint64 Generation=1,NextSequence=0;
    TManualPortPending<FGamePlatformProgressionSnapshotCompletion> BeginGetSnapshotCompletion;
    bool BeginGetSnapshot(FGamePlatformProgressionSnapshotCompletion Completion) override {return BeginGetSnapshotCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetSnapshot"),TEXT("read"),MoveTemp(Completion));}
    void CancelAllRequests() override {++Generation;BeginGetSnapshotCompletion.Reset();}
    FString PendingText() const {FString Out;if(BeginGetSnapshotCompletion)Out+=TEXT("；")+BeginGetSnapshotCompletion.Identity.Text();return Out;}
};
class FManualLiveOpsPort final : public IGamePlatformLiveOpsClientTransport
{
public:
    const FGuid ScopeId=FGuid::NewGuid();uint64 Generation=1,NextSequence=0;
    TManualPortPending<FGamePlatformLiveOpsCatalogCompletion> BeginGetCatalogCompletion;
    bool BeginGetCatalog(FGamePlatformLiveOpsCatalogCompletion Completion) override {return BeginGetCatalogCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetCatalog"),TEXT("read"),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformLiveOpsPlayerStateCompletion> BeginGetPlayerStateCompletion;
    bool BeginGetPlayerState(FGamePlatformLiveOpsPlayerStateCompletion Completion) override {return BeginGetPlayerStateCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetPlayerState"),TEXT("read"),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformLiveOpsClaimCompletion> BeginClaimSignInCompletion;
    bool BeginClaimSignIn(const FGuid& ClaimOperationId, FName CampaignId, FGamePlatformLiveOpsClaimCompletion Completion) override {return BeginClaimSignInCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginClaimSignIn"),ClaimOperationId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformLiveOpsClaimCompletion> BeginQueryClaimOperationCompletion;
    bool BeginQueryClaimOperation(const FGuid& ClaimOperationId, FGamePlatformLiveOpsClaimCompletion Completion) override {return BeginQueryClaimOperationCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginQueryClaimOperation"),ClaimOperationId.ToString(),MoveTemp(Completion));}
    void CancelAllRequests() override {++Generation;BeginGetCatalogCompletion.Reset();BeginGetPlayerStateCompletion.Reset();BeginClaimSignInCompletion.Reset();BeginQueryClaimOperationCompletion.Reset();}
    FString PendingText() const {FString Out;if(BeginGetCatalogCompletion)Out+=TEXT("；")+BeginGetCatalogCompletion.Identity.Text();if(BeginGetPlayerStateCompletion)Out+=TEXT("；")+BeginGetPlayerStateCompletion.Identity.Text();if(BeginClaimSignInCompletion)Out+=TEXT("；")+BeginClaimSignInCompletion.Identity.Text();if(BeginQueryClaimOperationCompletion)Out+=TEXT("；")+BeginQueryClaimOperationCompletion.Identity.Text();return Out;}
};
class FManualCommercePort final : public IGamePlatformCommerceClientTransport
{
public:
    const FGuid ScopeId=FGuid::NewGuid();uint64 Generation=1,NextSequence=0;
    TManualPortPending<FGamePlatformCommerceCatalogCompletion> BeginGetCatalogCompletion;
    bool BeginGetCatalog(FGamePlatformCommerceCatalogCompletion Completion) override {return BeginGetCatalogCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetCatalog"),TEXT("read"),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformCommerceIntentCompletion> BeginCreatePurchaseIntentCompletion;
    bool BeginCreatePurchaseIntent(FName OfferId, int32 Quantity, const FGuid& RequestId, FGamePlatformCommerceIntentCompletion Completion) override {return BeginCreatePurchaseIntentCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginCreatePurchaseIntent"),RequestId.ToString(),MoveTemp(Completion));}
    TManualPortPending<FGamePlatformCommerceOrderCompletion> BeginPurchaseCompletion;
    bool BeginPurchase(const FString& PurchaseIntentId, FGamePlatformCommerceOrderCompletion Completion) override {return BeginPurchaseCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginPurchase"),PurchaseIntentId,MoveTemp(Completion));}
    TManualPortPending<FGamePlatformCommerceOrderCompletion> BeginGetOrderCompletion;
    bool BeginGetOrder(const FString& OrderId, FGamePlatformCommerceOrderCompletion Completion) override {return BeginGetOrderCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginGetOrder"),OrderId,MoveTemp(Completion));}
    TManualPortPending<FGamePlatformCommerceOrderCompletion> BeginSubmitReceiptCompletion;
    bool BeginSubmitReceipt(const FString& OrderId, const FString& Receipt, FGamePlatformCommerceOrderCompletion Completion) override {return BeginSubmitReceiptCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginSubmitReceipt"),OrderId,MoveTemp(Completion));}
    TManualPortPending<FGamePlatformCommerceOrderCompletion> BeginReconcileOrderCompletion;
    bool BeginReconcileOrder(const FString& OrderId, FGamePlatformCommerceOrderCompletion Completion) override {return BeginReconcileOrderCompletion.Store(ScopeId,Generation,NextSequence,TEXT("BeginReconcileOrder"),OrderId,MoveTemp(Completion));}
    void CancelAllRequests() override {++Generation;BeginGetCatalogCompletion.Reset();BeginCreatePurchaseIntentCompletion.Reset();BeginPurchaseCompletion.Reset();BeginGetOrderCompletion.Reset();BeginSubmitReceiptCompletion.Reset();BeginReconcileOrderCompletion.Reset();}
    FString PendingText() const {FString Out;if(BeginGetCatalogCompletion)Out+=TEXT("；")+BeginGetCatalogCompletion.Identity.Text();if(BeginCreatePurchaseIntentCompletion)Out+=TEXT("；")+BeginCreatePurchaseIntentCompletion.Identity.Text();if(BeginPurchaseCompletion)Out+=TEXT("；")+BeginPurchaseCompletion.Identity.Text();if(BeginGetOrderCompletion)Out+=TEXT("；")+BeginGetOrderCompletion.Identity.Text();if(BeginSubmitReceiptCompletion)Out+=TEXT("；")+BeginSubmitReceiptCompletion.Identity.Text();if(BeginReconcileOrderCompletion)Out+=TEXT("；")+BeginReconcileOrderCompletion.Identity.Text();return Out;}
};
}
