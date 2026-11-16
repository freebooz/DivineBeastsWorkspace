// 真实Save服务人工调用；必须以-UserDir隔离到当前工程Saved验证根，不修改生产存储路径策略。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Interfaces/IGamePlatformSaveService.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace DivineBeasts::Demo
{
class FManualSaveProvider final : public IGamePlatformSaveProvider
{
public:
    explicit FManualSaveProvider(FName InNamespace):Namespace(InNamespace){}
    FName GetSaveNamespace() const override{return Namespace;}
    int32 GetCurrentSchemaVersion() const override{return 2;}
    FGamePlatformResult MigratePayload(int32 From,int32 To,TArray<uint8>& Payload) const override
    {
        if(From!=1 || To!=2)return ManualError(TEXT("Demo.SaveMigrationRange"),TEXT("夹具仅定义1到2迁移。"));
        Payload.Add(2);return FGamePlatformResult::Success();
    }
private:
    FName Namespace;
};
/** 原始IO不可强杀；Stop等待本次IO真实完成后在下一GT任务逐个删除自己的键。 */
struct FManualSaveState : TSharedFromThis<FManualSaveState>
{
    FDivineBeastsDemoRunToken Token;
    TWeakObjectPtr<UGameInstance> GI;
    FName Namespace;
    FString Profile;
    TArray<FGamePlatformSaveKey> OwnedKeys;
    TSharedPtr<FManualSaveProvider> Provider;
    FDivineBeastsDemoOperationCompletion Pending,StopDone;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    FGamePlatformSaveKey LastKey;
    FGamePlatformResult CleanupResult=FGamePlatformResult::Success();
    bool bPending=false,bStopping=false,bClosed=false,bProviderRegistered=false;
#if WITH_DEV_AUTOMATION_TESTS
    IGamePlatformSaveService* TestService=nullptr;
#endif
    IGamePlatformSaveService* Service() const{
#if WITH_DEV_AUTOMATION_TESTS
        if(TestService)return TestService;
#endif
        return GI.IsValid()?IGamePlatformSaveService::Get(*GI):nullptr;}

    void CleanNext()
    {
        if(bPending || bClosed)return;
        auto* Save=Service();
        if(!Save)
        {
            CleanupResult=ManualError(TEXT("Demo.SaveCleanupServiceMissing"),TEXT("服务在清理前失效；保留本次键诊断，未声明删除。"));
            auto Done=MoveTemp(StopDone);if(Done)Done(Token,CleanupResult);return;
        }
        if(!OwnedKeys.IsEmpty())
        {
            const auto Key=OwnedKeys.Last();bPending=true;FGamePlatformResult Accepted;
            const TSharedRef<FManualSaveState> Self=AsShared();
            const auto Handle=Save->DeleteRecordAsync(Key,[Self](const auto& Output)
            {
                Self->bPending=false;
                if(!Output.Result.IsSuccess())
                {
                    Self->CleanupResult=Output.Result;
                    auto Done=MoveTemp(Self->StopDone);if(Done)Done(Self->Token,Output.Result);
                    return; // 失败保留键，稍后Stop可重试，不丢句柄诊断。
                }
                Self->OwnedKeys.Pop();
                AsyncTask(ENamedThreads::GameThread,[Self](){Self->CleanNext();});
            },Accepted);
            if(!Handle.IsValid()){bPending=false;CleanupResult=Accepted;auto Done=MoveTemp(StopDone);if(Done)Done(Token,Accepted);}
            return;
        }
        if(bProviderRegistered)
        {
            const auto R=Save->UnregisterProvider(Namespace);
            if(!R.IsSuccess()){auto Done=MoveTemp(StopDone);if(Done)Done(Token,R);return;}
            bProviderRegistered=false;
        }
        bClosed=true;CleanupResult=FGamePlatformResult::Success();auto Done=MoveTemp(StopDone);if(Done)Done(Token,CleanupResult);
    }
    void RequestStop(FDivineBeastsDemoOperationCompletion Done)
    {
        if(StopDone && Done){Done(Token,ManualError(TEXT("Demo.SaveCleanupAlreadyInFlight"),TEXT("原清理IO仍在途，不能替换其完成端。")));return;}
        if(!Service() && (!OwnedKeys.IsEmpty() || bProviderRegistered || bPending))
        {if(Done)Done(Token,ManualError(TEXT("Demo.SaveCleanupServiceMissing"),TEXT("服务失效但仍有自有键/Provider/IO，未确认清理。")));return;}
        bStopping=true;CleanupResult=FGamePlatformResult::Success();StopDone=MoveTemp(Done);
        if(bClosed){auto C=MoveTemp(StopDone);if(C)C(Token,FGamePlatformResult::Success());return;}
        if(!bPending)
        {
            if(!Service() && OwnedKeys.IsEmpty() && !bProviderRegistered){bClosed=true;auto C=MoveTemp(StopDone);if(C)C(Token,FGamePlatformResult::Success());}
            else CleanNext();
        }
    }
};
class FManualSaveScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    FManualSaveScenario(UWorld& W,FName F):World(&W),Feature(F),S(MakeShared<FManualSaveState>()){}
    ~FManualSaveScenario() override {if(bPrepared)S->RequestStop({});}
    void Prepare(const FDivineBeastsDemoScenarioContext& C,FDivineBeastsDemoOperationCompletion Done) override
    {
        if(bBound){Done(C.Token,ManualError(TEXT("Demo.SaveSourceAlreadyBound"),TEXT("Save适配不可换Token来源。")));return;}
        bBound=true;Context=C;S->Token=C.Token;S->Publish=C.PublishObservation;
        const FName Adapter(*(FString(TEXT("PlatformServices."))+Feature.ToString()));
        if(!C.Token.IsValid() || C.World.Get()!=World.Get() || !World.IsValid() || World->bIsTearingDown ||
            C.Scenario.FeatureId!=Feature || C.Scenario.AdapterId!=Adapter || C.Scenario.ScenarioId!=FName(*(Feature.ToString()+TEXT(".Manual"))) || !ValidatePlatformManualContract(C.Scenario))
        {Done(C.Token,ManualError(TEXT("Demo.SaveSourceMismatch"),TEXT("Save来源世界、功能或固定适配身份不匹配。")));return;}
        S->GI=World->GetGameInstance();
        FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("../Saved/Validation/PluginManualUI20261027")));
        FString Actual=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
        FPaths::NormalizeDirectoryName(Allowed);FPaths::NormalizeDirectoryName(Actual);FPaths::CollapseRelativeDirectories(Allowed);FPaths::CollapseRelativeDirectories(Actual);
        if(!FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture")) || !FPaths::IsUnderDirectory(Actual,Allowed) || Actual.Equals(Allowed,ESearchCase::IgnoreCase))
        {Done(C.Token,ManualError(TEXT("Demo.SaveValidationHostRequired"),TEXT("请用隔离Standalone/Client的-UserDir指向本工作空间Saved/Validation/PluginManualUI20261027/SaveHost；未修改默认路径。")));return;}
        if(!S->Service()){Done(C.Token,ManualError(TEXT("Demo.SaveServiceMissing"),TEXT("本宿主无真实Save服务。")));return;}
        S->Namespace=FName(*(TEXT("Manual_")+C.Token.RunId.ToString(EGuidFormats::Digits)));
        S->Profile=C.Token.WorldId.ToString(EGuidFormats::Digits)+C.Token.RunId.ToString(EGuidFormats::Digits);
        S->Provider=MakeShared<FManualSaveProvider>(S->Namespace);bPrepared=true;
        Done(C.Token,ManualObserved(TEXT("真实Save服务与工程内隔离Saved路径已核对；无文件被写入。")));
    }
    void Execute(FName Op,const FManualValues& V,FDivineBeastsDemoOperationCompletion Done) override
    {
        if(Op==TEXT("ProbeSlotBusy") && Context.Scenario.Operations.Contains(Op) && bPrepared && !S->bStopping && S->bPending && S->Service())
        {
            FGamePlatformResult Accepted;const auto Handle=S->Service()->LoadRecordAsync(S->LastKey,[](const auto&){},Accepted);
            Done(Context.Token,Handle.IsValid()?ManualObserved(TEXT("原生Probe读取已受理，未伪造Busy。")):Accepted);return;
        }
        if(!bPrepared || S->bStopping || S->bPending || !World.IsValid() || World->bIsTearingDown || !S->Service())
        {Done(Context.Token,ManualError(TEXT("Demo.SaveInactiveOrBusy"),TEXT("Save未准备、忙碌、停止或世界/服务失效。")));return;}
        if(!Context.Scenario.Operations.Contains(Op) || V.Num()!=Context.Scenario.Parameters.Num())
        {Done(Context.Token,ManualError(TEXT("Demo.SaveOperationParameters"),TEXT("Save操作/参数不属于冻结合同。")));return;}
        for(int32 I=0;I<V.Num();++I)if(V[I].Type!=Context.Scenario.Parameters[I].DefaultValue.Type)
        {Done(Context.Token,ManualError(TEXT("Demo.SaveParameterType"),TEXT("Save参数类型不匹配。")));return;}
        FManualArguments A(Context,V);auto* Save=S->Service();FGamePlatformResult Accepted;
        if(Op==TEXT("RegisterProvider"))
        {
            Accepted=Save->RegisterProvider(S->Provider.ToSharedRef());if(Accepted.IsSuccess())S->bProviderRegistered=true;
            Done(Context.Token,Accepted);return;
        }
        if(Op==TEXT("UnregisterProvider"))
        {
            Accepted=Save->UnregisterProvider(S->Namespace);if(Accepted.IsSuccess())S->bProviderRegistered=false;
            Done(Context.Token,Accepted);return;
        }
        if(Op==TEXT("Inspect"))
        {
            const auto D=Save->GetDiagnostics();Done(Context.Token,ManualObserved(FString::Printf(TEXT("真实Save Pending=%d Providers=%d；本案例键=%d；不代表落盘成功"),
                D.PendingRequestCount,D.ProviderCount,S->OwnedKeys.Num())));return;
        }
        FGamePlatformSaveKey Key;Key.Namespace=S->Namespace;Key.ProfileKey=S->Profile;Key.SlotName=A.Id(TEXT("SlotName")).ToString();
        const bool StartOnly=Op==TEXT("StartWrite");const auto Self=S;S->Pending=StartOnly?FDivineBeastsDemoOperationCompletion{}:MoveTemp(Done);S->bPending=true;S->LastKey=Key;
        auto Callback=[Self](const FGamePlatformSaveOperationResult& Output)
        {
            Self->bPending=false;auto Completion=MoveTemp(Self->Pending);
            auto Result=Output.Result;
            Result.Message+=FString::Printf(TEXT("；Bytes=%d Schema=%d Backup=%d Migrated=%d；真实IO终态"),
                Output.Record.Payload.Num(),Output.Record.SchemaVersion,Output.bRecoveredFromBackup,Output.bMigrated);
            if(Self->Publish)Self->Publish(Self->Token,FString::Printf(TEXT("Save IOStatus=%d Error=%s Bytes=%d Schema=%d Backup=%d Migrated=%d"),
                int32(Output.Result.Status),*Output.Result.Code.ToString(),Output.Record.Payload.Num(),Output.Record.SchemaVersion,Output.bRecoveredFromBackup,Output.bMigrated));
            if(Completion)Completion(Self->Token,Self->bStopping?FGamePlatformResult::Cancelled(TEXT("本地案例停止；原IO已完成，随后清理自有记录。")):Result);
            if(Self->bStopping)AsyncTask(ENamedThreads::GameThread,[Self](){Self->CleanNext();});
        };
        FGamePlatformSaveRequestHandle Handle;
        if(Op==TEXT("Save") || StartOnly)
        {
            const int64 Byte=A.Int(TEXT("ByteValue")),Schema=A.Int(TEXT("SchemaVersion"));
            if(Byte<0 || Byte>255 || Schema<1 || Schema>MAX_int32)
            {S->bPending=false;auto C=MoveTemp(S->Pending);auto Error=ManualError(TEXT("Demo.SaveValueRange"),TEXT("ByteValue须0..255，Schema须正int32。"));if(C)C(Context.Token,Error);else if(StartOnly)Done(Context.Token,Error);return;}
            FGamePlatformSaveRecord R;R.Key=Key;R.SchemaVersion=static_cast<int32>(Schema);R.Payload={static_cast<uint8>(Byte)};
            Handle=Save->SaveRecordAsync(R,MoveTemp(Callback),Accepted);
            if(Handle.IsValid() && !S->OwnedKeys.ContainsByPredicate([&Key](const auto& X){return X.SlotName==Key.SlotName;}))S->OwnedKeys.Add(Key);
        }
        else if(Op==TEXT("Load"))Handle=Save->LoadRecordAsync(Key,MoveTemp(Callback),Accepted);
        else if(Op==TEXT("Delete"))Handle=Save->DeleteRecordAsync(Key,MoveTemp(Callback),Accepted);
        else Accepted=ManualError(TEXT("Demo.SaveOperation"),TEXT("操作不属于Save白名单。"));
        if(!Handle.IsValid()){S->bPending=false;auto C=MoveTemp(S->Pending);if(C)C(Context.Token,Accepted);else if(StartOnly)Done(Context.Token,Accepted);}
        else if(StartOnly)Done(Context.Token,ManualObserved(TEXT("真实Save写IO已受理；终态由PublishObservation通知，可手动ProbeSlotBusy或Stop。")));
    }
    void Stop(FDivineBeastsDemoOperationCompletion Done) override
    {bPrepared=false;if(!bBound){Done(Context.Token,FGamePlatformResult::Success());return;}S->RequestStop(MoveTemp(Done));}
private:
    TWeakObjectPtr<UWorld> World;
    const FName Feature;
    FDivineBeastsDemoScenarioContext Context;
    TSharedRef<FManualSaveState> S;
    bool bBound=false,bPrepared=false;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformSaveScenario(UWorld& W,FName F)
{return MakeUnique<FManualSaveScenario>(W,F);}
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/DivineBeastsDemoManualSaveProbe.inl"
#endif

}

