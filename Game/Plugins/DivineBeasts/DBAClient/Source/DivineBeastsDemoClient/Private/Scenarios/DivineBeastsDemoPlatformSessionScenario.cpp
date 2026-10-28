// Session原生状态机人工夹具；Transport只接收操作者事实，不执行ClientTravel，不伪称服务器准入。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "GamePlatformSessionClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo
{
class FManualSessionTransport final : public IGamePlatformSessionTransport
{
public:
    FGamePlatformSessionTransportCallbacks Callbacks;
    FGamePlatformSessionDisconnectedCallback Disconnected;
    FGuid ActiveOperation;uint64 Generation=0;
    void BeginTransfer(const FGamePlatformSessionTransferRequest& R,FGamePlatformSessionTransportCallbacks C) override
    {++Generation;ActiveOperation=R.TransferOperationId;Callbacks=MoveTemp(C);}
    void CancelTransfer(const FGuid& Id) override{if(ActiveOperation==Id){++Generation;Callbacks={};ActiveOperation.Invalidate();}}
    void CompleteTransfer(const FGuid& Id,const FGamePlatformSessionConnectionBinding&) override{if(ActiveOperation==Id){++Generation;Callbacks={};}}
    void SetDisconnectedCallback(FGamePlatformSessionDisconnectedCallback C) override{Disconnected=MoveTemp(C);}
    void LeaveSession(const FGamePlatformSessionConnectionBinding&) override{++Generation;Callbacks={};}
};
struct FManualSessionView
{
    FDivineBeastsDemoRunToken Token;TWeakObjectPtr<UWorld> World;FGamePlatformSessionSnapshot Snapshot;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    bool bAlive=true;uint64 Events=0;
    FString Text() const{return FString::Printf(TEXT("隔离Session State=%d Recovery=%d Admission=%d CanRetry=%d Error=%s Events=%llu；不代表真实网络/准入"),
        int32(Snapshot.State),int32(Snapshot.RecoveryState),Snapshot.bAdmissionConfirmed,Snapshot.bCanRetry,*Snapshot.ErrorCode.ToString(),Events);}
};
class FManualSessionScenario final : public FPlatformManualBase
{
public:
    FManualSessionScenario(UWorld& W,FName F):FPlatformManualBase(W,F),View(MakeShared<FManualSessionView>()){}
    ~FManualSessionScenario() override{CleanupService();}
private:
    FGamePlatformResult PrepareService() override
    {
        if(!GEngine)return ManualError(TEXT("Demo.SessionEngineMissing"),TEXT("Session夹具需要Engine宿主。"));
        Host.Reset(NewObject<UGameInstance>(GEngine));Session.Reset(NewObject<UGamePlatformSessionClientSubsystem>(Host.Get()));
        FSubsystemCollection<UGameInstanceSubsystem> Collection;Session->Initialize(Collection);
        Transport=MakeShared<FManualSessionTransport>();Session->SetTransport(Transport);
        Session->SetAuthenticationContext(TEXT("ManualAccount"),FGuid::NewGuid());
        Binding.AssignmentId=TEXT("ManualAssignment");Binding.GameSessionId=TEXT("ManualGameSession");Binding.ServerInstanceId=TEXT("ManualServer");
        Binding.ServerBootId=TEXT("ManualBoot");Binding.WorldId=TEXT("ManualWorld");Binding.ProtocolVersion=TEXT("1.0.0");Binding.SessionEpoch=1;
        Operation=FGuid::NewGuid();View->Token=Context.Token;View->World=SourceWorld;View->Publish=Context.PublishObservation;View->Snapshot=Session->GetSnapshot();
        TWeakPtr<FManualSessionView> Weak(View);
        Changed=Session->OnSessionChanged().AddLambda([Weak](const auto& S){auto V=Weak.Pin();if(!V || !V->bAlive || !V->World.IsValid() || V->World->bIsTearingDown)return;
            V->Snapshot=S;++V->Events;if(V->Publish)V->Publish(V->Token,V->Text());});
        if(View->Publish)View->Publish(View->Token,View->Text());
        return ManualObserved(View->Text());
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;
        if(Session.IsValid()){Session->OnSessionChanged().Remove(Changed);Session->Deinitialize();Session.Reset();}
        Transport.Reset();Host.Reset();return FGamePlatformResult::Success();
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        FGamePlatformResult R;
        if(Op==TEXT("NewOperation"))
        {
            const auto State=View->Snapshot.State;
            if(View->Snapshot.bRecoveryRequired || (State!=EGamePlatformSessionTransferState::Idle && State!=EGamePlatformSessionTransferState::Ready &&
                State!=EGamePlatformSessionTransferState::Failed && State!=EGamePlatformSessionTransferState::Cancelled && State!=EGamePlatformSessionTransferState::TimedOut))
                R=ManualError(TEXT("Demo.SessionOperationStillOwned"),TEXT("活动或不确定会话操作必须先完成/对账，不能换新身份。"));
            else {Operation=FGuid::NewGuid();R=ManualObserved(TEXT("已生成新的独立会话操作身份；原不确定操作未被替换。"));}
        }
        else if(Op==TEXT("BeginJoin") || Op==TEXT("BeginTransfer") || Op==TEXT("Reconnect"))
        {
            FGamePlatformSessionTransferRequest Request;Request.TransferOperationId=Operation;Request.AssignmentId=Binding.AssignmentId;
            Request.GameServerId=Binding.ServerInstanceId;Request.ServerRoleId=TEXT("Village");Request.ExperienceId=TEXT("Village.Training");Request.WorldId=Binding.WorldId;
            Request.Endpoint=TEXT("127.0.0.1:7777");Request.TransferTicket=TEXT("MANUAL_PORT_FIXTURE_NOT_A_CREDENTIAL");Request.TicketId=TEXT("ManualTicket");
            Request.SessionId=TEXT("ManualSession");Request.ExpectedBinding=Binding;Request.TimeoutSeconds=A.Number(TEXT("TimeoutSeconds"));
            if(Op==TEXT("Reconnect"))Session->Reconnect(Request,R);
            else if(Op==TEXT("BeginTransfer"))Session->BeginOperation(EGamePlatformSessionIntent::Transfer,Request,R);
            else Session->BeginOperation(EGamePlatformSessionIntent::Join,Request,R);
        }
        else if(Op==TEXT("PrepareBinding"))
        {if(const auto Callback=Transport->Callbacks.OnBindingPrepared){const uint64 SentGeneration=Transport->Generation;Callback(Binding);if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("回调中运行已停止。")));return;}
                if(Transport->Generation!=SentGeneration && Context.PublishObservation)Context.PublishObservation(Context.Token,TEXT("已交付的Transport代次已结束；不继续读取旧回调。"));
                R=ManualObserved(TEXT("夹具已交付BindingPrepared。"));}else R=ManualError(TEXT("Demo.SessionNoPreparedCallback"),TEXT("没有等待Binding的Transport回调。"));}
        else if(Op==TEXT("CommitTravel"))
        {if(const auto Callback=Transport->Callbacks.OnTravelCommitted){const uint64 SentGeneration=Transport->Generation;Callback(Binding);if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("回调中运行已停止。")));return;}
                if(Transport->Generation!=SentGeneration && Context.PublishObservation)Context.PublishObservation(Context.Token,TEXT("已交付的Transport代次已结束；不继续读取旧回调。"));
                R=ManualObserved(TEXT("夹具交付不可回滚边界事实；没有实际Travel。"));}else R=ManualError(TEXT("Demo.SessionNoTravelCallback"),TEXT("无有效Travel回调。"));}
        else if(Op==TEXT("NetworkFact") || Op==TEXT("AdmissionFact"))
        {if(const auto Callback=Transport->Callbacks.OnFact){const uint64 SentGeneration=Transport->Generation;Callback(Op==TEXT("NetworkFact")?EGamePlatformSessionTransferFact::NetworkConnected:EGamePlatformSessionTransferFact::AdmissionConfirmed,Binding);if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("事实回调中运行已停止。")));return;}
                if(Transport->Generation!=SentGeneration && Context.PublishObservation)Context.PublishObservation(Context.Token,TEXT("Transport事实已收敛到新代次，不复用旧回调。"));
                R=ManualObserved(TEXT("通过Transport合同交付夹具事实；不构成真实联机证据。"));}else R=ManualError(TEXT("Demo.SessionNoFactCallback"),TEXT("无有效Transport事实回调。"));}
        else if(Op==TEXT("WorldFact") || Op==TEXT("ControllerFact") || Op==TEXT("TryForgeAdmission"))
            Session->ReportLocalFact(Operation,Op==TEXT("WorldFact")?EGamePlatformSessionTransferFact::TargetWorldLoaded:
                Op==TEXT("ControllerFact")?EGamePlatformSessionTransferFact::ControllerReady:EGamePlatformSessionTransferFact::AdmissionConfirmed,Binding,R);
        else if(Op==TEXT("FailTransport"))
        {if(const auto Callback=Transport->Callbacks.OnFailed){const uint64 SentGeneration=Transport->Generation;Callback(TEXT("Demo.ManualTransportFailed"),TEXT("人工Port故障。"));if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("故障回调中运行已停止。")));return;}
                if(Transport->Generation!=SentGeneration && Context.PublishObservation)Context.PublishObservation(Context.Token,TEXT("故障已终止原Transport代次。"));
                R=ManualObserved(TEXT("故障交付原生Session。"));}else R=ManualError(TEXT("Demo.SessionNoFailureCallback"),TEXT("无有效失败回调。"));}
        else if(Op==TEXT("CancelTransfer"))Session->CancelTransfer(R);
        else if(Op==TEXT("NotifyDisconnected"))Session->NotifyDisconnected(Binding,R);
        else if(Op==TEXT("ResolveAbsent"))Session->ResolveRemoteState(Operation,{},R);
        else if(Op==TEXT("ResolveExisting"))Session->ResolveRemoteState(Operation,Binding,R);
        else if(Op==TEXT("LeaveSession"))Session->LeaveSession(R);
        else if(Op==TEXT("Inspect"))R=FGamePlatformResult::Success();
        else R=ManualError(TEXT("Demo.SessionOperation"),TEXT("Session操作未在白名单。"));
        if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("信号回调中本运行已停止。")));return;}
        R.Message+=TEXT("；")+View->Text();Done(Context.Token,R);
    }
    TStrongObjectPtr<UGameInstance> Host;
    TStrongObjectPtr<UGamePlatformSessionClientSubsystem> Session;
    TSharedPtr<FManualSessionTransport> Transport;
    TSharedRef<FManualSessionView> View;
    FGamePlatformSessionConnectionBinding Binding;
    FGuid Operation;
    FDelegateHandle Changed;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformSessionScenario(UWorld& W,FName F)
{return MakeUnique<FManualSessionScenario>(W,F);}
}

