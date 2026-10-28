// 中央Data租约与普通薄加载器人工操作；所有对象使用本世界的真实Data服务和完整租约。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Validation/GamePlatformDefinitionValidation.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Async/Async.h"
namespace DivineBeasts::Demo
{
struct FManualDataState
{
    FDivineBeastsDemoRunToken Token;
    TWeakObjectPtr<UGameInstance> GI;
    TWeakObjectPtr<UWorld> World;
    FGamePlatformDataLease Lease;
    TSharedPtr<FStreamableHandle> ThinHandle;
    FDivineBeastsDemoOperationCompletion Pending;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    bool bAlive=true,bDefinition=false,bPending=false,bThinPending=false;
    uint64 Generation=0;
    IGamePlatformDataService* Service() const{return GI.IsValid()?IGamePlatformDataService::Get(*GI):nullptr;}
    FGamePlatformResult Release()
    {
        ++Generation;bPending=false;bThinPending=false;
        FGamePlatformAssetLoader::Cancel(ThinHandle);ThinHandle.Reset();
        auto* D=Service();
        if(Lease.IsValid())
        {
            if(!D)return ManualError(TEXT("Demo.DataCleanupUnavailable"),TEXT("Data失效；完整租约保留用于清理诊断。"));
            const auto R=bDefinition?D->ReleaseDefinition(Lease):D->ReleaseResources(Lease);
            if(!R.IsSuccess())return R;
            Lease={};
        }
        return FGamePlatformResult::Success();
    }
};
class FManualDataScenario final : public FPlatformManualBase
{
public:
    FManualDataScenario(UWorld& W,FName F):FPlatformManualBase(W,F),S(MakeShared<FManualDataState>()){}
    ~FManualDataScenario() override {S->bAlive=false;S->Pending={};S->Release();}
private:
    FGamePlatformResult PrepareService() override
    {
        S->Token=Context.Token;S->World=SourceWorld;S->Publish=Context.PublishObservation;S->GI=Instance();
        return S->Service()?FGamePlatformResult::Success():ManualError(TEXT("Demo.DataServiceMissing"),TEXT("需要本世界已初始化的中央Data服务。"));
    }
    FGamePlatformResult CleanupService() override
    {
        S->bAlive=false;auto Done=MoveTemp(S->Pending);const auto R=S->Release();
        if(Done)Done(S->Token,FGamePlatformResult::Cancelled(TEXT("本案例取消，只撤销自身Data需求。")));
        return R;
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        auto* D=S->Service();
        if(Op==TEXT("CancelThinLoader"))
        {
            if(!S->bThinPending){Done(Context.Token,ManualObserved(TEXT("无在途薄加载；Data定义/资源申请不受本取消影响。")));return;}
            const bool WasPending=S->bThinPending;++S->Generation;S->bPending=false;S->bThinPending=false;
            auto NativeHandle=MoveTemp(S->ThinHandle);auto Previous=MoveTemp(S->Pending);
            FGamePlatformAssetLoader::Cancel(NativeHandle);NativeHandle.Reset();
            if(Previous)Previous(Context.Token,FGamePlatformResult::Cancelled(TEXT("本薄加载操作取消，Data租约保留。")));
            Done(Context.Token,ManualObserved(WasPending?TEXT("在途薄加载已取消；Data需求保留，不伪造Ready。"):TEXT("薄加载已经结束；取消幂等，不改Data租约。")));return;
        }
        if(Op==TEXT("Release"))
        {auto Previous=MoveTemp(S->Pending);const auto R=S->Release();if(Previous)Previous(Context.Token,FGamePlatformResult::Cancelled(TEXT("本次Data需求释放取消在途等待。")));Done(Context.Token,R);return;}
        if(!D){Done(Context.Token,ManualError(TEXT("Demo.DataServiceMissing"),TEXT("Data服务失效。")));return;}
        if(S->bPending && Op!=TEXT("Inspect")){Done(Context.Token,ManualError(TEXT("Demo.DataBusy"),TEXT("其它Data操作仍在途；可CancelThinLoader或Release。")));return;}
        if(Op==TEXT("Inspect"))
        {
            const auto* Def=D->GetLoadedDefinition(S->Lease);
            Done(Context.Token,ManualObserved(FString::Printf(TEXT("State=%d；DefinitionReady=%d；SchemaRange=%d..%d"),
                int32(D->GetLeaseState(S->Lease)),Def!=nullptr,Def?Def->GetMinimumReadableSchemaVersion():0,Def?Def->GetMaximumReadableSchemaVersion():0)));return;
        }
        if(Op==TEXT("ValidateLoaded"))
        {
            const auto* Def=D->GetLoadedDefinition(S->Lease);
            Done(Context.Token,Def?Def->ValidateDefinition():ManualError(TEXT("Demo.DataNotReady"),TEXT("本租约没有就绪定义。")));return;
        }
        FGamePlatformId Id;
        if(Op==TEXT("AcquireDefinition") || Op==TEXT("ResolveUniqueSource"))
        {
            if(A.Int(TEXT("LogicalVersion"))<1 || A.Int(TEXT("LogicalVersion"))>MAX_int32 || !FGamePlatformId::TryCreate(A.Id(TEXT("Namespace")).ToString(),A.Id(TEXT("Name")).ToString(),static_cast<int32>(A.Int(TEXT("LogicalVersion"))),Id))
            {Done(Context.Token,ManualError(TEXT("Demo.DefinitionIdentity"),TEXT("平台拒绝定义逻辑身份。")));return;}
        }
        const FPrimaryAssetId DefinitionId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),FName(*Id.ToString()));
        if(Op==TEXT("ResolveUniqueSource"))
        {FSoftObjectPath Path;auto R=ResolveUniqueGamePlatformDefinitionSource(DefinitionId,Path);if(R.IsSuccess())R.Message=TEXT("唯一实际源=")+Path.ToString();Done(Context.Token,R);return;}
        if(Op==TEXT("TestThinLoader"))
        {
            if(D->GetLeaseState(S->Lease)!=EGamePlatformDataRequestState::Succeeded || S->bDefinition)
            {Done(Context.Token,ManualError(TEXT("Demo.ThinLoaderNeedsLease"),TEXT("先取得本案例普通资源Data租约，不能绕过Data所有权。")));return;}
            const auto Path=A.Asset(TEXT("Resource"));
            if(!S->Lease.ResourcePaths.Contains(Path))
            {Done(Context.Token,ManualError(TEXT("Demo.ThinLoaderPathMismatch"),TEXT("薄加载测试路径须由当前Data租约持有。")));return;}
            S->bPending=true;S->bThinPending=true;const uint64 G=++S->Generation;TWeakPtr<FManualDataState> Weak(S);
            auto Native=FGamePlatformAssetLoader::RequestAsyncLoad({Path},FStreamableDelegate::CreateLambda([Weak,G]()
            {
                AsyncTask(ENamedThreads::GameThread,[Weak,G]()
                {
                    auto P=Weak.Pin();if(!P || !P->bAlive || !P->bPending || G!=P->Generation || !P->World.IsValid() || P->World->bIsTearingDown)return;
                    P->bPending=false;P->bThinPending=false;P->ThinHandle.Reset();if(P->Publish)P->Publish(P->Token,TEXT("真实薄加载完成；Data租约继续保活；这是原操作终态。"));
                    auto C=MoveTemp(P->Pending);if(C)C(P->Token,ManualObserved(TEXT("真实薄加载完成。")));
                });
            }));
            if(!S->bAlive || S->Generation!=G){FGamePlatformAssetLoader::Cancel(Native);Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("启动回调中已停止。")));return;}
            S->ThinHandle=MoveTemp(Native);
            if(!S->ThinHandle){S->bPending=false;S->bThinPending=false;Done(Context.Token,ManualError(TEXT("Demo.ThinLoaderRejected"),TEXT("薄加载未接纳。")));return;}
            Done(Context.Token,ManualObserved(TEXT("薄加载发起已受理，未就绪；UI写操作已结束，可按CancelThinLoader；真正完成由事件报告。")));
            return;
        }
        if(Op!=TEXT("AcquireDefinition") && Op!=TEXT("AcquireResource"))
        {Done(Context.Token,ManualError(TEXT("Demo.DataOperation"),TEXT("Data操作不在编译期白名单。")));return;}
        if(S->Lease.IsValid())
        {Done(Context.Token,ManualError(TEXT("Demo.DataLeaseAlreadyOwned"),TEXT("先显式Release本租约，再请求新资源。")));return;}
        auto* Caller=SourceWorld->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
        if(!Caller){Done(Context.Token,ManualError(TEXT("Demo.DataCallerMissing"),TEXT("缺本世界合法租约调用者。")));return;}
        S->bDefinition=Op==TEXT("AcquireDefinition");S->bPending=true;const uint64 G=++S->Generation;
        TWeakPtr<FManualDataState> Weak(S);
        auto Complete=[Weak,G](const FGamePlatformDataLease& L,const FGamePlatformResult& Result)
        {
            auto P=Weak.Pin();if(!P || !P->bAlive || !P->bPending || P->Generation!=G || !P->World.IsValid() || P->World->bIsTearingDown)return;
            const auto& Expected=P->Lease;
            const bool Match=L.ScopeId==Expected.ScopeId && L.LeaseId==Expected.LeaseId && L.Generation==Expected.Generation &&
                L.IssuerProof==Expected.IssuerProof && L.DefinitionId==Expected.DefinitionId && L.Bundles==Expected.Bundles && L.ResourcePaths==Expected.ResourcePaths;
            if(!Match){if(P->Publish)P->Publish(P->Token,TEXT("Demo.DataLeaseIdentityMismatch：忽略不匹配完成，保留本请求等待/释放资格。"));return;}
            P->bPending=false;if(P->Publish)P->Publish(P->Token,FString::Printf(TEXT("Data状态=%d；结果=%d；错误码=%s"),
                P->Service()?int32(P->Service()->GetLeaseState(P->Lease)):int32(EGamePlatformDataRequestState::Invalid),int32(Result.Status),*Result.Code.ToString()));
            auto C=MoveTemp(P->Pending);if(C)C(P->Token,Match?Result:ManualError(TEXT("Demo.DataLeaseIdentityMismatch"),TEXT("完成租约与完整签发身份不匹配。")));
        };
        FGamePlatformResult Accepted;
        S->Lease=S->bDefinition?D->AcquireDefinition(DefinitionId,UGamePlatformDefinitionBase::StaticClass(),{},EGamePlatformDataLifetime::World,Caller,MoveTemp(Complete),Accepted):
            D->AcquireResources({A.Asset(TEXT("Resource"))},EGamePlatformDataLifetime::World,Caller,MoveTemp(Complete),Accepted);
        if(!Accepted.IsSuccess()){S->bPending=false;Done(Context.Token,Accepted);}
        else Done(Context.Token,ManualObserved(TEXT("真实Data请求发起已受理，未就绪；UI不占用等待，可Release；完整租约终态由正式延后通知报告。")));
    }
    TSharedRef<FManualDataState> S;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformDataScenario(UWorld& W,FName F)
{return MakeUnique<FManualDataScenario>(W,F);}
}

