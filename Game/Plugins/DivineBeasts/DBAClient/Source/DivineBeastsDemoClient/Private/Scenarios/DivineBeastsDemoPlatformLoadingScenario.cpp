// Loading人工任务源只提供用户交付的事实，依赖、进度聚合、截止与清理由真实Loading实现。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/GameInstance.h"
namespace DivineBeasts::Demo
{
struct FManualLoadingFact {FGamePlatformLoadingTaskUpdate Update;int32 ReleaseCount=0;};
class FManualLoadingTask final : public IGamePlatformLoadingTask
{
public:
    explicit FManualLoadingTask(TSharedRef<FManualLoadingFact> InFact):Fact(InFact){}
    FGamePlatformResult Start(UGameInstance&,const FGamePlatformLoadingTaskSpec&) override
    {bReleased=false;return FGamePlatformResult::Success();}
    FGamePlatformLoadingTaskUpdate Poll() override{return Fact->Update;}
    bool IsReadyToUse() const override{return !bReleased && Fact->Update.State==EGamePlatformLoadingTaskUpdate::Succeeded;}
    void Release() override{bReleased=true;++Fact->ReleaseCount;}
private:
    TSharedRef<FManualLoadingFact> Fact;bool bReleased=false;
};
struct FManualLoadingView
{
    FDivineBeastsDemoRunToken Token;
    TWeakObjectPtr<UWorld> World;
    FGamePlatformLoadingSnapshot Snapshot;
    FDivineBeastsDemoOperationCompletion Pending;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    bool bAlive=true;
    uint64 Events=0;
};
class FManualLoadingScenario final : public FPlatformManualBase
{
public:
    FManualLoadingScenario(UWorld& W,FName F):FPlatformManualBase(W,F),Fact(MakeShared<FManualLoadingFact>()),SecondFact(MakeShared<FManualLoadingFact>()),View(MakeShared<FManualLoadingView>()){}
    ~FManualLoadingScenario() override{CleanupService();}
private:
    IGamePlatformLoadingService* Service() const{return Instance()?IGamePlatformLoadingService::Get(*Instance()):nullptr;}
    FGamePlatformResult PrepareService() override
    {
        auto* L=Service();
        if(!L)return ManualError(TEXT("Demo.LoadingMissing"),TEXT("需要本GameInstance真实Loading服务。"));
        const auto Existing=L->GetLoadingSnapshot();
        if(Existing.State==EGamePlatformLoadingState::Running || (Existing.Handle.IsValid() && Existing.bResourcesHeld))
            return ManualError(TEXT("Demo.LoadingOwnedByAnotherOperation"),TEXT("当前GI已有未释放加载操作；不接管或释放其资源。"));
        View->Token=Context.Token;View->World=SourceWorld;View->Snapshot=Existing;View->Publish=Context.PublishObservation;
        FactoryId=FName(*(TEXT("ManualTask_")+Context.Token.RunId.ToString(EGuidFormats::Digits)));SecondFactoryId=FName(*(FactoryId.ToString()+TEXT("_B")));
        return FGamePlatformResult::Success();
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;auto Pending=MoveTemp(View->Pending);FGamePlatformResult R=FGamePlatformResult::Success();
        if(auto* L=Service())
        {
            if(Subscription.IsValid()){if(!L->UnsubscribeLoadingState(Subscription))R=ManualError(TEXT("Demo.LoadingUnsubscribe"),TEXT("本订阅撤销失败，保留句柄诊断。"));else Subscription={};}
            if(Handle.IsValid()){const auto Released=L->ReleaseLoadingOperation(Handle);if(!Released.IsSuccess())R=Released;else Handle={};}
            if(SecondFactory.IsValid()){const auto Released=L->UnregisterTaskFactory(SecondFactory);if(!Released.IsSuccess())R=Released;else SecondFactory={};}
            if(Factory.IsValid()){const auto Released=L->UnregisterTaskFactory(Factory);if(!Released.IsSuccess())R=Released;else Factory={};}
        }
        else if(Handle.IsValid() || Factory.IsValid() || SecondFactory.IsValid())R=ManualError(TEXT("Demo.LoadingCleanupMissing"),TEXT("服务失效；本句柄仍保留，未宣称释放成功。"));
        if(Pending)Pending(Context.Token,FGamePlatformResult::Cancelled(TEXT("Loading人工等待取消；不撤销任何服务器事务。")));
        return R;
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        auto* L=Service();if(!L){Done(Context.Token,ManualError(TEXT("Demo.LoadingMissing"),TEXT("Loading服务已失效。")));return;}
        FGamePlatformResult R;
        if((Op==TEXT("SetProgress") || Op==TEXT("CompleteTask") || Op==TEXT("FailTask")) && (A.Int(TEXT("TaskIndex"))<0 || A.Int(TEXT("TaskIndex"))>1))
        {Done(Context.Token,ManualError(TEXT("Demo.LoadingTaskIndex"),TEXT("TaskIndex只能0或1，不映射未知索引。")));return;}
        if(Op==TEXT("RegisterTask"))
        {
            if(Factory.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.LoadingFactoryOwned"),TEXT("本工厂已登记。")));return;}
            const auto Source=Fact;
            Factory=L->RegisterTaskFactory(FactoryId,[Source](){return MakeUnique<FManualLoadingTask>(Source);},R);
            if(R.IsSuccess()){const auto Other=SecondFact;SecondFactory=L->RegisterTaskFactory(SecondFactoryId,[Other](){return MakeUnique<FManualLoadingTask>(Other);},R);}
        }
        else if(Op==TEXT("StartLoading"))
        {
            if(Handle.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.LoadingHandleOwned"),TEXT("先释放本操作。")));return;}
            Fact->Update={};SecondFact->Update={};
            FGamePlatformLoadingOperationSpec Spec;Spec.Purpose=TEXT("ManualFixture");Spec.TimeoutSeconds=A.Number(TEXT("TimeoutSeconds"));
            Spec.TargetWorldPackage=UWorld::RemovePIEPrefix(SourceWorld->GetOutermost()->GetName());
            FGamePlatformLoadingTaskSpec Task;Task.TaskId=TEXT("Manual");Task.TaskType=FactoryId;Task.TimeoutSeconds=Spec.TimeoutSeconds;
            Spec.Tasks.Add(Task);
            FGamePlatformLoadingTaskSpec OtherTask;OtherTask.TaskId=TEXT("ManualSecond");OtherTask.TaskType=SecondFactoryId;OtherTask.TimeoutSeconds=Spec.TimeoutSeconds;OtherTask.Dependencies.Add(Task.TaskId);Spec.Tasks.Add(OtherTask);
            if(Feature==TEXT("GamePlatformLoading.02"))
            {FGamePlatformLoadingTaskSpec W;W.TaskId=TEXT("World");W.TaskType=TEXT("WorldPresence");W.TimeoutSeconds=Spec.TimeoutSeconds;Spec.Tasks.Add(W);}
            auto* Caller=SourceWorld->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
            Handle=L->StartLoadingOperation(Spec,Caller,R);
            if(Handle.IsValid())
            {
                TWeakPtr<FManualLoadingView> Weak(View);
                Subscription=L->SubscribeLoadingState(Handle,Caller,[Weak](const FGamePlatformLoadingSnapshot& S)
                {
                    auto V=Weak.Pin();if(!V || !V->bAlive || !V->World.IsValid() || V->World->bIsTearingDown)return;
                    V->Snapshot=S;++V->Events;
                    if(V->Publish)V->Publish(V->Token,FString::Printf(TEXT("Loading State=%d Progress=%.3f Error=%s ResourcesHeld=%d"),
                        int32(S.State),S.OverallProgress01,*S.Result.Code.ToString(),S.bResourcesHeld));
                    if(V->Pending && S.State!=EGamePlatformLoadingState::Running)
                    {auto Completion=MoveTemp(V->Pending);Completion(V->Token,S.Result);}
                });
            }
            if(R.IsSuccess())R.Message=TEXT("真实Loading受理；尚未Ready，等待人工任务/真实世界门禁。");
        }
        else if(Op==TEXT("SetProgress"))
        {if(A.Number(TEXT("Progress"))<0 || A.Number(TEXT("Progress"))>1){Done(Context.Token,ManualError(TEXT("Demo.LoadingProgressRange"),TEXT("Progress须0..1，不夹紧。")));return;}
            const auto Target=A.Int(TEXT("TaskIndex"))==0?Fact:SecondFact;
            Target->Update.Progress01=A.Number(TEXT("Progress"));R=ManualObserved(TEXT("已更新夹具任务事实；真实Loading将在自身调度发布进度。"));}
        else if(Op==TEXT("CompleteTask") || Op==TEXT("FailTask"))
        {
            if(!Handle.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.LoadingNotStarted"),TEXT("先StartLoading。")));return;}
            // 不阻塞人工下一步世界声明；任务完成不意味着WorldPresence完成。
            const auto Target=A.Int(TEXT("TaskIndex"))==0?Fact:SecondFact;
            Target->Update.State=Op==TEXT("CompleteTask")?EGamePlatformLoadingTaskUpdate::Succeeded:EGamePlatformLoadingTaskUpdate::Failed;
            Target->Update.Progress01=Op==TEXT("CompleteTask")?1.0:0.0;Target->Update.Error=Op==TEXT("FailTask")?FName(TEXT("Demo.ManualTaskFailed")):NAME_None;
            R=ManualObserved(TEXT("夹具任务终态事实已提交；操作整体终态以Loading真实事件为准。"));
        }
        else if(Op==TEXT("ReportWorldOperable"))R=L->ReportWorldOperable(Handle,*SourceWorld);
        else if(Op==TEXT("CancelLoading"))R=L->CancelLoadingOperation(Handle);
        else if(Op==TEXT("ReleaseLoading"))
        {
            R=L->ReleaseLoadingOperation(Handle);
            if(R.IsSuccess()){if(Subscription.IsValid()){if(!L->UnsubscribeLoadingState(Subscription))R=ManualError(TEXT("Demo.LoadingReleaseUnsubscribe"),TEXT("操作资源已释放，但自有订阅撤销失败，保留句柄。"));else Subscription={};}Handle={};}
        }
        else if(Op==TEXT("Inspect"))R=FGamePlatformResult::Success();
        else R=ManualError(TEXT("Demo.LoadingOperation"),TEXT("Loading操作未支持。"));
        R.Message+=FString::Printf(TEXT("；State=%d Progress=%.3f ReadyToPlay=%d Events=%llu Released=%d"),
            int32(View->Snapshot.State),View->Snapshot.OverallProgress01,L->IsReadyToPlay(Handle),View->Events,Fact->ReleaseCount+SecondFact->ReleaseCount);
        Done(Context.Token,R);
    }
    TSharedRef<FManualLoadingFact> Fact,SecondFact;
    TSharedRef<FManualLoadingView> View;
    FName FactoryId,SecondFactoryId;
    FGamePlatformLoadingRegistration Factory,SecondFactory,Subscription;
    FGamePlatformLoadingHandle Handle;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformLoadingScenario(UWorld& W,FName F)
{return MakeUnique<FManualLoadingScenario>(W,F);}
}

