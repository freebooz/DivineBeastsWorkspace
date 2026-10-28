// Editor专用Server人工桥；显式手控Provider，仅在无其它Provider时登记，普通Client不链接ServerOnly。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#if WITH_EDITOR
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"
#include "Features/IModularFeatures.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo
{
struct FManualControlRequest
{
    FName Kind;FGuid RequestId;uint64 Sequence=0,Generation=0;
    FGamePlatformServerControlCompletion Completion;
};
class FManualControlProvider final : public IGamePlatformServerControlProvider,public TSharedFromThis<FManualControlProvider>
{
public:
    FGamePlatformServerInstanceInfo Expected;bool bBound=false,bClosed=false;uint64 Generation=1,NextSequence=0;
    TArray<FManualControlRequest> Pending;
    bool Matches(const FGamePlatformServerInstanceInfo& I) const
    {return bBound && I.GameId==Expected.GameId && I.GameServerId==Expected.GameServerId && I.ServerBootId==Expected.ServerBootId &&
        I.ServerRoleId==Expected.ServerRoleId && I.ExperienceId==Expected.ExperienceId && I.WorldId==Expected.WorldId &&
        I.RegionId==Expected.RegionId && I.BuildVersion==Expected.BuildVersion && I.ProtocolVersion==Expected.ProtocolVersion &&
        I.PublicEndpoint==Expected.PublicEndpoint && I.Capacity==Expected.Capacity && I.ClusterId==Expected.ClusterId && I.NodeId==Expected.NodeId;}
    bool Bind(const FGamePlatformServerInstanceInfo& I){if(bBound)return Matches(I);if(!I.IsValid())return false;Expected=I;bBound=true;return true;}
    void Store(FName Kind,const FGamePlatformServerInstanceInfo& I,FGamePlatformServerControlCompletion C)
    {
        const auto Keep=AsShared();
        if(bClosed || !Matches(I)){C(false,TEXT("Demo.ForeignServerInstanceRejected"));return;}
        FManualControlRequest R;R.Kind=Kind;R.RequestId=FGuid::NewGuid();R.Sequence=++NextSequence;R.Generation=Generation;R.Completion=MoveTemp(C);Pending.Add(MoveTemp(R));
    }
    void RegisterInstance(const FGamePlatformServerInstanceInfo& I,FGamePlatformServerControlCompletion C) override{Store(TEXT("Register"),I,MoveTemp(C));}
    void SendHeartbeat(const FGamePlatformServerInstanceInfo& I,int32,const FString&,FGamePlatformServerControlCompletion C) override{Store(TEXT("Heartbeat"),I,MoveTemp(C));}
    void PublishReady(const FGamePlatformServerInstanceInfo& I,FGamePlatformServerControlCompletion C) override{Store(TEXT("Ready"),I,MoveTemp(C));}
    void BeginDrain(const FGamePlatformServerInstanceInfo& I,FGamePlatformServerControlCompletion C) override{Store(TEXT("Drain"),I,MoveTemp(C));}
    bool Take(FName Kind,int64 Sequence,FManualControlRequest& Out)
    {
        for(int32 I=0;I<Pending.Num();++I)if(Sequence>0 && Pending[I].Kind==Kind && Pending[I].Sequence==static_cast<uint64>(Sequence) && Pending[I].Generation==Generation)
        {Out=MoveTemp(Pending[I]);Pending.RemoveAt(I);return true;}return false;
    }
    FString Text() const {FString Out;for(const auto& R:Pending)Out+=FString::Printf(TEXT("；Control=%s Seq=%llu Gen=%llu Request=%s"),*R.Kind.ToString(),R.Sequence,R.Generation,*R.RequestId.ToString());return Out;}
    void Close(){bClosed=true;++Generation;auto Local=MoveTemp(Pending);for(auto& R:Local){auto C=MoveTemp(R.Completion);if(C)C(false,TEXT("Demo.ProviderClosed"));}}
};
struct FManualAdmissionRequest
{
    FName Kind;FGuid RequestId,Operation,Connection;uint64 Sequence=0,Generation=0,ConnectionGeneration=0;
    FGamePlatformServerAdmissionTarget Target;FGamePlatformServerAdmissionCompletion Completion;
};
class FManualAdmissionProvider final : public IGamePlatformServerAdmissionProvider,public TSharedFromThis<FManualAdmissionProvider>
{
public:
    FGamePlatformServerAdmissionTarget Expected;FGuid AllowedOperation;
    bool bClosed=false;uint64 Generation=1,NextSequence=0;
    TArray<FManualAdmissionRequest> Pending;TMap<FGuid,FGamePlatformServerVerifiedAdmission> Issued;
    bool Matches(const FGamePlatformServerAdmissionTarget& T) const
    {return T.GameServerId==Expected.GameServerId && T.ServerBootId==Expected.ServerBootId && T.WorldId==Expected.WorldId &&
        T.ExperienceId==Expected.ExperienceId && T.ProtocolVersion==Expected.ProtocolVersion && T.ServerStartGeneration==Expected.ServerStartGeneration;}
    static FGamePlatformServerAdmissionResult Denied(){FGamePlatformServerAdmissionResult R;R.ErrorCode=TEXT("Demo.ForeignAdmissionSourceRejected");return R;}
    void ValidateAdmission(const FGamePlatformServerAdmissionTarget& T,FGuid Id,uint64 G,FGamePlatformServerAdmissionProof Proof,FGamePlatformServerAdmissionCompletion C) override
    {
        const auto Keep=AsShared();
        if(bClosed || !Matches(T) || Proof.OperationId!=AllowedOperation){C(Denied());return;}
        FManualAdmissionRequest R;R.Kind=TEXT("Validate");R.RequestId=FGuid::NewGuid();R.Sequence=++NextSequence;R.Generation=Generation;
        R.Target=T;R.Connection=Id;R.ConnectionGeneration=G;R.Operation=Proof.OperationId;R.Completion=MoveTemp(C);Pending.Add(MoveTemp(R));
    }
    void ReleaseAdmission(const FGamePlatformServerVerifiedAdmission& A,FGamePlatformServerAdmissionCompletion C) override
    {
        const auto Keep=AsShared();const auto* Original=Issued.Find(A.AdmissionId);
        if(bClosed || !Original || A.ConnectionId!=Original->ConnectionId || A.ConnectionGeneration!=Original->ConnectionGeneration ||
            !A.MatchesTarget(Expected)){if(C)C(Denied());return;}
        FManualAdmissionRequest R;R.Kind=TEXT("Release");R.RequestId=A.AdmissionId;R.Sequence=++NextSequence;R.Generation=Generation;
        R.Target=Expected;R.Connection=A.ConnectionId;R.ConnectionGeneration=A.ConnectionGeneration;R.Operation=AllowedOperation;R.Completion=MoveTemp(C);Pending.Add(MoveTemp(R));
    }
    void CancelOperation(const FGuid& Id) override{Pending.RemoveAll([&](const auto& R){return R.Operation==Id;});}
    bool Take(FName Kind,int64 Sequence,FManualAdmissionRequest& Out)
    {
        for(int32 I=0;I<Pending.Num();++I)if(Sequence>0 && Pending[I].Kind==Kind && Pending[I].Sequence==static_cast<uint64>(Sequence) && Pending[I].Generation==Generation)
        {Out=MoveTemp(Pending[I]);Pending.RemoveAt(I);return true;}return false;
    }
    FString Text() const {FString Out;for(const auto& R:Pending)Out+=FString::Printf(TEXT("；Admission=%s Seq=%llu Gen=%llu Request=%s Operation=%s"),*R.Kind.ToString(),R.Sequence,R.Generation,*R.RequestId.ToString(),*R.Operation.ToString());return Out;}
    void Close(){bClosed=true;++Generation;auto Local=MoveTemp(Pending);Issued.Reset();for(auto& R:Local){auto C=MoveTemp(R.Completion);if(C)C(Denied());}}
};
struct FManualServerView
{
    FDivineBeastsDemoRunToken Token;TWeakObjectPtr<UWorld> World;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    bool bAlive=true;uint64 Events=0;
    void Changed(FString Text){if(!bAlive || !World.IsValid() || World->bIsTearingDown)return;++Events;
        if(Publish)Publish(Token,Text+FString::Printf(TEXT("；Events=%llu；仅本地Port夹具"),Events));}
};
class FManualServerScenario final : public FPlatformManualBase
{
public:
    FManualServerScenario(UWorld& W,FName F):FPlatformManualBase(W,F),View(MakeShared<FManualServerView>()){}
    ~FManualServerScenario() override{CleanupService();}
private:
    FGamePlatformResult PrepareService() override
    {
        if(!Instance() || SourceWorld->GetNetMode()==NM_Client)
            return ManualError(TEXT("Demo.ServerAuthorityWorldRequired"),TEXT("Editor桥需要同实例真实权威World，普通客户端不能申请服务器资格。"));
        View->Token=Context.Token;View->World=SourceWorld;View->Publish=Context.PublishObservation;
        auto& Features=IModularFeatures::Get();TWeakPtr<FManualServerView> Weak(View);
        if(Feature==TEXT("GamePlatformServer.03"))
        {
            if(Features.GetModularFeatureImplementations<IGamePlatformServerAdmissionProvider>(IGamePlatformServerAdmissionProvider::GetModularFeatureName()).Num()!=0)
                return ManualError(TEXT("Demo.ServerAdmissionProviderAlreadyPresent"),TEXT("已有准入Provider；无沙箱不得替换或叠加。"));
            AdmissionProvider=MakeShared<FManualAdmissionProvider>();Features.RegisterModularFeature(IGamePlatformServerAdmissionProvider::GetModularFeatureName(),AdmissionProvider.Get());bAdmissionRegistered=true;
            Admission.Reset(NewObject<UGamePlatformServerAdmissionSubsystem>(Instance()));
            Controller=SourceWorld->SpawnActor<APlayerController>();
            if(!Controller.IsValid())return ManualError(TEXT("Demo.AdmissionControllerMissing"),TEXT("自有真实Controller无法生成。"));
            AdmissionChanged=Admission->OnAdmissionChanged().AddLambda([Weak](const APlayerController*,const auto&,bool Valid)
                {if(auto V=Weak.Pin())V->Changed(Valid?TEXT("原生准入已发布自有非敏感投影。"):TEXT("原生准入自有投影已撤销。"));});
        }
        else
        {
            if(Features.GetModularFeatureImplementations<IGamePlatformServerControlProvider>(IGamePlatformServerControlProvider::GetModularFeatureName()).Num()!=0)
                return ManualError(TEXT("Demo.ServerControlProviderAlreadyPresent"),TEXT("已有控制面Provider；无开发沙箱不得替换或叠加。"));
            ControlProvider=MakeShared<FManualControlProvider>();Features.RegisterModularFeature(IGamePlatformServerControlProvider::GetModularFeatureName(),ControlProvider.Get());bControlRegistered=true;
            Lifecycle.Reset(NewObject<UGamePlatformServerLifecycleSubsystem>(Instance()));
            LifecycleChanged=Lifecycle->OnLifecycleChanged().AddLambda([Weak](const auto& Snapshot)
                {if(auto V=Weak.Pin())V->Changed(FString::Printf(TEXT("Lifecycle State=%d Error=%s"),int32(Snapshot.State),*Snapshot.ErrorCode.ToString()));});
        }
        if(AdmissionProvider){AdmissionProvider->Expected=MakeTarget();AdmissionProvider->AllowedOperation=OperationId;}
        View->Changed(TEXT("独立Editor真实Server对象已准备；Provider仅接受本RunId实例/目标/操作身份，不承接其它GI请求。"));return FGamePlatformResult::Success();
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;
        if(Lifecycle.IsValid()){Lifecycle->OnLifecycleChanged().Remove(LifecycleChanged);Lifecycle->StopHeartbeatPump();Lifecycle->Deinitialize();Lifecycle.Reset();}
        if(Admission.IsValid()){Admission->OnAdmissionChanged().Remove(AdmissionChanged);Admission->ResetTarget(TEXT("ManualFixtureStop"));Admission->Deinitialize();Admission.Reset();}
        if(Controller.IsValid()){Controller->Destroy();Controller.Reset();}
        if(ControlProvider)ControlProvider->Close();if(AdmissionProvider)AdmissionProvider->Close();
        auto& Features=IModularFeatures::Get();
        if(bControlRegistered){Features.UnregisterModularFeature(IGamePlatformServerControlProvider::GetModularFeatureName(),ControlProvider.Get());bControlRegistered=false;}
        if(bAdmissionRegistered){Features.UnregisterModularFeature(IGamePlatformServerAdmissionProvider::GetModularFeatureName(),AdmissionProvider.Get());bAdmissionRegistered=false;}
        ControlProvider.Reset();AdmissionProvider.Reset();return FGamePlatformResult::Success();
    }
    FGamePlatformServerAdmissionTarget MakeTarget() const
    {
        FGamePlatformServerAdmissionTarget T;T.GameServerId=Context.Token.RunId.ToString(EGuidFormats::Digits);
        T.ServerBootId=Context.Token.WorldId.ToString(EGuidFormats::Digits);T.WorldId=TEXT("ManualWorld_")+Context.Token.RunId.ToString(EGuidFormats::Digits);
        T.ExperienceId=TEXT("Village.Training");T.ProtocolVersion=TEXT("1.0.0");T.ServerStartGeneration=Context.Token.Generation;return T;
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        FGamePlatformResult R;bool Accepted=false;
        if(Lifecycle.IsValid())
        {
            if(Op==TEXT("RegisterInstance"))
            {
                FGamePlatformServerInstanceInfo I;I.GameId=TEXT("ManualGame");I.GameServerId=Context.Token.RunId.ToString(EGuidFormats::Digits);I.ServerBootId=Context.Token.WorldId.ToString(EGuidFormats::Digits);
                I.ServerRoleId=TEXT("Village");I.ExperienceId=TEXT("Village.Training");I.WorldId=TEXT("ManualWorld_")+Context.Token.RunId.ToString(EGuidFormats::Digits);I.RegionId=TEXT("ManualRegion");
                I.BuildVersion=TEXT("manual-fixture");I.ProtocolVersion=static_cast<int32>(A.Int(TEXT("Protocol")));I.PublicEndpoint=TEXT("127.0.0.1:7777");
                I.Capacity=static_cast<int32>(A.Int(TEXT("Capacity")));
                Accepted=ControlProvider->Bind(I) && Lifecycle->RegisterInstance(I);
            }
            else if(Op==TEXT("MarkReady"))Accepted=Lifecycle->MarkReady();
            else if(Op==TEXT("Heartbeat"))Accepted=Lifecycle->SendHeartbeat(static_cast<int32>(A.Int(TEXT("Players"))));
            else if(Op==TEXT("StartHeartbeat"))
            {const int32 Players=static_cast<int32>(A.Int(TEXT("Players")));Accepted=Lifecycle->StartHeartbeatPump([Players](){return Players;},static_cast<float>(A.Number(TEXT("IntervalSeconds"))));}
            else if(Op==TEXT("StopHeartbeat")){Lifecycle->StopHeartbeatPump();Accepted=true;}
            else if(Op==TEXT("BeginDrain"))Accepted=Lifecycle->BeginDrain();
            else if(Op==TEXT("CompleteDrain"))Accepted=Lifecycle->CompleteDrain();
            else if(Op==TEXT("DeliverControl"))
            {
                const auto P=ControlProvider;FManualControlRequest Request;
                if(P->Take(A.Id(TEXT("ControlKind")),A.Int(TEXT("ControlSequence")),Request))
                {auto C=MoveTemp(Request.Completion);C(A.Bool(TEXT("Success")),A.Bool(TEXT("Success"))?NAME_None:FName(TEXT("Demo.ManualControlFailure")));Accepted=true;}
            }
            else if(Op==TEXT("Inspect"))Accepted=true;
            if(!Alive() || !Lifecycle.IsValid()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("生命周期事件中本案例已停止。")));return;}
            const auto S=Lifecycle->GetSnapshot();
            R=Accepted?ManualObserved(FString::Printf(TEXT("原生命令受理；State=%d AckPlayers=%d AckGeneration=%llu；非真实控制面注册证据"),
                int32(S.State),Lifecycle->GetLastAcknowledgedHeartbeatPlayers(),Lifecycle->GetLastAcknowledgedHeartbeatGeneration())+ControlProvider->Text()):
                ManualError(TEXT("Demo.ServerControlRejected"),TEXT("真实服务器对象拒绝操作或无待交付回调。"));
        }
        else if(Admission.IsValid())
        {
            const auto Target=MakeTarget();
            if(Op==TEXT("ConfigureTarget"))Accepted=Admission->ConfigureTarget(Target);
            else if(Op==TEXT("BeginAdmission"))
            {
                FGamePlatformServerAdmissionProof Proof;Proof.OperationId=OperationId;Proof.ReservationId=TEXT("ManualReservation");Proof.AttemptId=TEXT("ManualAttempt");Proof.Credential.Init(0x7f,32);
                const auto V=View;
                Accepted=Admission->BeginAdmission(*Controller,MoveTemp(Proof),[V](const auto& Result){V->Changed(Result.bSucceeded?TEXT("Native准入夹具完成成功。"):TEXT("Native准入夹具完成失败。"));});
            }
            else if(Op==TEXT("ReleaseAdmission")){const auto V=View;Accepted=Admission->ReleaseAdmission(*Controller,[V](const auto& Result){V->Changed(Result.bSucceeded?TEXT("Native准入释放夹具完成。"):TEXT("Native准入释放失败。"));});}
            else if(Op==TEXT("ResetTarget")){Admission->ResetTarget(TEXT("ManualTargetReset"));Accepted=true;}
            else if(Op==TEXT("DeliverAdmission"))
            {
                const auto P=AdmissionProvider;FManualAdmissionRequest Request;
                if(P->Take(A.Id(TEXT("AdmissionKind")),A.Int(TEXT("AdmissionSequence")),Request))
                {
                    FGamePlatformServerAdmissionResult Result;Result.bSucceeded=A.Bool(TEXT("Success"));Result.ErrorCode=Result.bSucceeded?NAME_None:FName(TEXT("Demo.ManualAdmissionDenied"));
                    if(Result.bSucceeded && Request.Kind==TEXT("Validate"))
                    {
                        auto& V=Result.Admission;const auto T=Request.Target;
                        V.AdmissionId=FGuid::NewGuid();V.ConnectionId=Request.Connection;V.PlayerId=TEXT("ManualPlayer");V.SessionId=TEXT("ManualSession");
                        V.GameSessionId=TEXT("ManualGameSession");V.AssignmentId=TEXT("ManualAssignment");V.ReservationId=TEXT("ManualReservation");
                        V.ServerInstanceId=T.GameServerId;V.ServerBootId=T.ServerBootId;V.WorldId=T.WorldId;V.ExperienceId=T.ExperienceId;V.ProtocolVersion=T.ProtocolVersion;
                        V.ConnectionGeneration=Request.ConnectionGeneration;V.SessionEpoch=1;V.AuthorityUntil=FDateTime::UtcNow()+FTimespan::FromMinutes(5);
                    }
                    if(Result.bSucceeded && Request.Kind==TEXT("Validate"))P->Issued.Add(Result.Admission.AdmissionId,Result.Admission);
                    if(Result.bSucceeded && Request.Kind==TEXT("Release"))P->Issued.Remove(Request.RequestId);
                    auto C=MoveTemp(Request.Completion);if(C)C(MoveTemp(Result));Accepted=true;
                }
            }
            else if(Op==TEXT("Inspect"))Accepted=true;
            if(!Alive() || !Admission.IsValid() || !Controller.IsValid()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("准入事件中本案例已停止。")));return;}
            FGamePlatformServerVerifiedAdmission Verified;
            const bool Valid=Admission->GetVerifiedAdmission(*Controller,Verified);
            R=Accepted?ManualObserved(FString::Printf(TEXT("原生准入操作受理；Verified=%d；无真实控制面授权证据"),Valid)+AdmissionProvider->Text()):
                ManualError(TEXT("Demo.ServerAdmissionRejected"),TEXT("真实准入对象拒绝操作或无待交付响应。"));
        }
        else R=ManualError(TEXT("Demo.ServerBridgeInactive"),TEXT("Editor Server桥未准备。"));
        Done(Context.Token,R);
    }
    TSharedRef<FManualServerView> View;
    TStrongObjectPtr<UGamePlatformServerLifecycleSubsystem> Lifecycle;
    TStrongObjectPtr<UGamePlatformServerAdmissionSubsystem> Admission;
    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<FManualControlProvider> ControlProvider;
    TSharedPtr<FManualAdmissionProvider> AdmissionProvider;
    bool bControlRegistered=false,bAdmissionRegistered=false;
    FDelegateHandle LifecycleChanged,AdmissionChanged;
    FGuid OperationId=FGuid::NewGuid();
};
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/DivineBeastsDemoManualServerProbe.inl"
#endif

TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformServerScenario(UWorld& W,FName F)
{return MakeUnique<FManualServerScenario>(W,F);}
}
#else
namespace DivineBeasts::Demo
{
class FManualServerHostRequired final : public FPlatformManualBase
{
public:FManualServerHostRequired(UWorld& W,FName F):FPlatformManualBase(W,F){}
private:
    FGamePlatformResult PrepareService() override{return ManualError(TEXT("Demo.ServerEditorHostRequired"),TEXT("ServerOnly能力需要Editor独立权威夹具/开发Server宿主；普通Client不链接或运行Server模块。"));}
    void ExecuteService(FName,const FManualArguments&,FDivineBeastsDemoOperationCompletion Done) override
    {Done(Context.Token,ManualError(TEXT("Demo.ServerHostMissing"),TEXT("Server宿主未准备。")));}
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformServerScenario(UWorld& W,FName F)
{return MakeUnique<FManualServerHostRequired>(W,F);}
}
#endif

