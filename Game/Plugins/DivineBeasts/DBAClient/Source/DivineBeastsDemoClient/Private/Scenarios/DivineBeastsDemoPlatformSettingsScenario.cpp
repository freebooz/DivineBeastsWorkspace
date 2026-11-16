// 真实Settings服务人工台：专属验证Host与RunId Provider/User键，类型化修改及精确自身恢复。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Interfaces/IGamePlatformSettingsService.h"
#include "Interfaces/IGamePlatformSettingsProvider.h"
#include "Interfaces/IGamePlatformDeviceSettingsService.h"
#include "Features/IModularFeatures.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Engine/GameInstance.h"
#include "Async/Async.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
namespace DivineBeasts::Demo
{
class FManualSettingsProvider final : public IGamePlatformSettingsProvider
{
public:
    explicit FManualSettingsProvider(const FString& Suffix):Prefix(TEXT("Manual.")+Suffix){}
    FName GetProviderId() const override{return FName(*Prefix);}
    FName Key(const TCHAR* Kind) const{return FName(*(Prefix+TEXT(".")+Kind));}
    FName Category() const{return FName(*Prefix);}
    void GetSettingDescriptors(TArray<FGamePlatformSettingDescriptor>& Out) const override
    {
        for(int32 I=0;I<3;++I)
        {
            FGamePlatformSettingDescriptor D;D.Category=Category();D.SettingId=Key(I==0?TEXT("Number"):I==1?TEXT("Integer"):TEXT("Boolean"));
            D.RuntimeScope=EGamePlatformSettingRuntimeScope::Client;D.PersistenceScope=EGamePlatformSettingScope::User;
            D.bDevelopmentOnly=true;
            D.ValueType=I==0?EGamePlatformSettingValueType::Number:I==1?EGamePlatformSettingValueType::Integer:EGamePlatformSettingValueType::Boolean;
            D.DefaultValue=I==0?FGamePlatformSettingValue::MakeNumber(1.0):I==1?FGamePlatformSettingValue::MakeInteger(1):FGamePlatformSettingValue::MakeBool(false);
            D.DefaultLayer=EGamePlatformSettingLayer::ProviderDefault;Out.Add(D);
        }
    }
private:
    FString Prefix;
};
struct FManualSettingsView
{
    TWeakObjectPtr<UWorld> World;
    FGamePlatformSettingsSnapshot Snapshot;
    FGamePlatformDeviceSettingsSnapshot Device;
    FDivineBeastsDemoRunToken Token;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    bool bAlive=true,bStopping=false,bResumeScheduled=false,bWaitingIO=false;
    FDivineBeastsDemoOperationCompletion StopDone;
    TFunction<void()> ResumeCleanup;
    uint64 Events=0;
};
class FManualSettingsScenario final : public FPlatformManualBase
{
public:
    FManualSettingsScenario(UWorld& W,FName F):FPlatformManualBase(W,F),View(MakeShared<FManualSettingsView>()){}
    ~FManualSettingsScenario() override
    {
        View->bAlive=false;View->ResumeCleanup={};View->StopDone={};CleanupService();
        // 原生ModularFeature持裸机制指针，析构必须撤销本Provider，即使世界退出后服务已不可用。
        if(bProviderRegistered){IModularFeatures::Get().UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(),Provider.Get());bProviderRegistered=false;}
    }
private:
    void QueueCleanupResume()
    {
        if(View->bResumeScheduled || !View->bStopping || !View->ResumeCleanup)return;
        View->bResumeScheduled=true;TWeakPtr<FManualSettingsView> Weak(View);
        AsyncTask(ENamedThreads::GameThread,[Weak](){auto V=Weak.Pin();if(!V)return;V->bResumeScheduled=false;
            if(!V->bAlive || !V->bStopping || !V->ResumeCleanup)return;auto Resume=V->ResumeCleanup;Resume();});
    }
    void ContinueStop()
    {
        const auto Snapshot=Service()?Service()->GetSnapshot():FGamePlatformSettingsSnapshot{};
        if(View->bWaitingIO && !Snapshot.bLoading && !Snapshot.bSaveInFlight && Snapshot.LastResult.Status!=EGamePlatformResultStatus::NotExecuted && !Snapshot.LastResult.IsSuccess())
        {
            View->bAlive=false;View->bStopping=false;View->bWaitingIO=false;View->ResumeCleanup={};auto Done=MoveTemp(View->StopDone);
            if(Done)Done(Context.Token,Snapshot.LastResult);return;
        }
        View->bWaitingIO=false;
        const auto Result=CleanupService();
        if(Result.Code==TEXT("Demo.SettingsIOStillRunning") || Result.Code==TEXT("Demo.SettingsCleanupSaveInFlight"))
        {
            View->bAlive=true;View->bWaitingIO=true;
            if(auto* S=Service()){const auto Now=S->GetSnapshot();if(!Now.bLoading && !Now.bSaveInFlight)QueueCleanupResume();}
            return;
        }
        View->bStopping=false;View->ResumeCleanup={};auto Done=MoveTemp(View->StopDone);if(Done)Done(Context.Token,Result);
    }
    void StopService(FDivineBeastsDemoOperationCompletion Done) override
    {
        if(View->bStopping && View->StopDone){Done(Context.Token,ManualError(TEXT("Demo.SettingsCleanupAlreadyInFlight"),TEXT("本次清理已在等待真实IO事件。")));return;}
        View->bStopping=true;View->bWaitingIO=false;View->StopDone=MoveTemp(Done);View->ResumeCleanup=[this](){ContinueStop();};ContinueStop();
    }
    IGamePlatformSettingsService* Service() const{return Instance()?IGamePlatformSettingsService::Get(*Instance()):nullptr;}
    IGamePlatformDeviceSettingsService* Device() const{return Instance()?IGamePlatformDeviceSettingsService::Get(*Instance()):nullptr;}
    FGamePlatformResult PrepareService() override
    {
        if(SourceWorld->WorldType!=EWorldType::Game)return ManualError(TEXT("Demo.SettingsStandaloneRequired"),TEXT("设备/用户设置测试须独立Standalone/Client Game World，避免PIE多世界共享设备状态。"));
        const FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("../Saved/Validation/PluginManualUI20261027")));
        if(!FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture")) ||
            !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()),Allowed))
            return ManualError(TEXT("Demo.SettingsValidationHostRequired"),TEXT("设置写入需独立-GamePlatformManualFixture Host及工程验证目录下-UserDir。"));
        auto* S=Service();auto* D=Device();
        if(!S || !D)return ManualError(TEXT("Demo.SettingsServiceMissing"),TEXT("真实Runtime/Device设置服务不可用。"));
        auto* Host=Instance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();
        if(!Host)return ManualError(TEXT("Demo.SettingsHostCollection"),TEXT("真实GI未装配受控宿主子系统。"));
        const auto Exclusive=Host->Acquire(Context.Token,*SourceWorld);if(!Exclusive.IsSuccess())return Exclusive;HostLease=Host;
        if(!S->TryGetUserContextKey(OriginalUserKey))return ManualError(TEXT("Demo.SettingsContextCapture"),TEXT("无法捕获原用户键，拒绝任何用户上下文修改。"));
        bOriginalContextCaptured=true;
        OwnUserKey=TEXT("Manual_")+Context.Token.RunId.ToString(EGuidFormats::Digits);
        const auto Existing=S->GetSnapshot();
        if(Existing.bLoading || Existing.bDirty || Existing.bSaveInFlight || D->GetSnapshot().bPreviewActive || D->GetSnapshot().bHasStagedChanges)
            return ManualError(TEXT("Demo.SettingsHostBusy"),TEXT("宿主有其他未完成设置事务；不接管其更改。"));
        View->World=SourceWorld;View->Token=Context.Token;View->Publish=Context.PublishObservation;View->Snapshot=Existing;View->Device=D->GetSnapshot();OriginalDevice=View->Device.Current;
        Provider=MakeUnique<FManualSettingsProvider>(Context.Token.RunId.ToString(EGuidFormats::Digits));
        IModularFeatures::Get().RegisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(),Provider.Get());bProviderRegistered=true;
        auto* Owner=SourceWorld->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
        TWeakPtr<FManualSettingsView> Weak(View);FGamePlatformResult R;
        SettingsSub=S->Subscribe(Owner,[Weak](const auto&,const auto& Snapshot){auto V=Weak.Pin();if(!V || !V->bAlive || !V->World.IsValid())return;V->Snapshot=Snapshot;++V->Events;if(V->Publish)V->Publish(V->Token,FString::Printf(TEXT("Settings Revision=%lld Loading=%d Dirty=%d SaveInFlight=%d Error=%s"),
                Snapshot.Revision,Snapshot.bLoading,Snapshot.bDirty,Snapshot.bSaveInFlight,*Snapshot.LastResult.Code.ToString()));
                if(V->bStopping && !Snapshot.bLoading && !Snapshot.bSaveInFlight && !V->bResumeScheduled && V->ResumeCleanup)
                {
                    V->bResumeScheduled=true;
                    AsyncTask(ENamedThreads::GameThread,[Weak](){auto S=Weak.Pin();if(!S)return;S->bResumeScheduled=false;
                        if(!S->bAlive || !S->bStopping || !S->ResumeCleanup)return;auto Resume=S->ResumeCleanup;Resume();});
                }
            },R);
        if(!R.IsSuccess())return R;
        DeviceSub=D->Subscribe(Owner,[Weak](const auto& Snapshot){auto V=Weak.Pin();if(!V || !V->bAlive || !V->World.IsValid())return;V->Device=Snapshot;++V->Events;if(V->Publish)V->Publish(V->Token,FString::Printf(TEXT("Device Revision=%lld Preview=%d Staged=%d"),
                Snapshot.Revision,Snapshot.bPreviewActive,Snapshot.bHasStagedChanges));},R);
        return R;
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;
        if(!bProviderRegistered && !Provider && !bOwnUserContext && !bDeviceTouched && !SettingsSub.IsValid() && !DeviceSub.IsValid())
        {
            if(HostLease.IsValid()){const auto R=HostLease->Release(Context.Token);if(!R.IsSuccess())return R;HostLease.Reset();}
            return FGamePlatformResult::Success(); // Prepare的只读拒绝不能接管原宿主Dirty/Loading。
        }
        auto* S=Service();auto* D=Device();
        if(!S || !D)return (bProviderRegistered || bOwnUserContext || bUserTouched || SettingsSub.IsValid() || DeviceSub.IsValid())?
            ManualError(TEXT("Demo.SettingsCleanupMissing"),TEXT("服务失效且仍有自有用户记录/上下文/订阅，不能确认清理。")):FGamePlatformResult::Success();
        FString CurrentKey;
        if(bOriginalContextCaptured && (!S->TryGetUserContextKey(CurrentKey) || CurrentKey!=(bOwnUserContext?OwnUserKey:OriginalUserKey)))
            return ManualError(TEXT("Demo.SettingsContextOwnershipChanged"),TEXT("用户上下文已被外部更换，保留原身份，不覆盖或保存外部上下文。"));
        const auto Current=S->GetSnapshot();
        if(Current.bLoading || Current.bSaveInFlight)
            return ManualError(TEXT("Demo.SettingsIOStillRunning"),TEXT("本设置IO仍在途；不能假称取消落盘。请等待真实事件后重试Stop。"));
        if(bDeviceTouched)
        {
            FGamePlatformResult R;
            if(D->GetSnapshot().bPreviewActive)R=D->CancelPreview();
            else {R=D->StageDeviceSettings(OriginalDevice);if(R.IsSuccess())R=D->ApplyStagedSettings(EGamePlatformDeviceSettingsApplyMode::Commit);}
            if(!R.IsSuccess())return R;bDeviceTouched=false;
        }
        if(Provider)
        {
            const auto SessionReset=S->ResetCategory(Provider->Category(),EGamePlatformSettingLayer::Session);if(!SessionReset.IsSuccess())return SessionReset;
            if(bOwnUserContext){const auto UserReset=S->ResetCategory(Provider->Category(),EGamePlatformSettingLayer::User);if(!UserReset.IsSuccess())return UserReset;}
            auto R=S->Apply(EGamePlatformSettingsChangeReason::Reset);if(!R.IsSuccess())return R;
        }
        if(bOwnUserContext)
        {
            if(S->GetSnapshot().bDirty){const auto Saved=S->Save();if(!Saved.IsSuccess())return Saved;
                return ManualError(TEXT("Demo.SettingsCleanupSaveInFlight"),TEXT("自有User层已清理，保存已受理；等待真实完成后重试Stop，不把受理称落盘成功。"));}
            const auto R=S->SwitchUserContext(OriginalUserKey);if(!R.IsSuccess())return R;bOwnUserContext=false;
            if(S->GetSnapshot().bLoading)return ManualError(TEXT("Demo.SettingsIOStillRunning"),TEXT("退出本RunId用户上下文已受理，等待真实读取终态。"));
        }
        if(bProviderRegistered){IModularFeatures::Get().UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(),Provider.Get());bProviderRegistered=false;}
        if(S->GetSnapshot().bLoading)return ManualError(TEXT("Demo.SettingsIOStillRunning"),TEXT("自有Provider撤销后的真实Reload仍在途。"));
        if(S->GetSnapshot().bDirty || S->GetSnapshot().bSaveInFlight)return ManualError(TEXT("Demo.SettingsIOStillRunning"),TEXT("自有设置删除/拓扑尚未清洁，不能完成Stop。"));
        bUserTouched=false;
        if(SettingsSub.IsValid()){if(!S->Unsubscribe(SettingsSub))return ManualError(TEXT("Demo.SettingsUnsubscribe"),TEXT("本Runtime设置订阅撤销失败。"));SettingsSub={};}
        if(DeviceSub.IsValid()){if(!D->Unsubscribe(DeviceSub))return ManualError(TEXT("Demo.DeviceUnsubscribe"),TEXT("本Device设置订阅撤销失败。"));DeviceSub={};}
        if(bProviderRegistered){IModularFeatures::Get().UnregisterModularFeature(IGamePlatformSettingsProvider::GetModularFeatureName(),Provider.Get());bProviderRegistered=false;}
        if(HostLease.IsValid()){const auto R=HostLease->Release(Context.Token);if(!R.IsSuccess())return R;HostLease.Reset();}
        return FGamePlatformResult::Success();
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        auto* S=Service();auto* D=Device();
        if(!S || !D || !Provider){Done(Context.Token,ManualError(TEXT("Demo.SettingsMissing"),TEXT("设置服务或自有Provider失效。")));return;}
        FGamePlatformResult R;const auto Layer=A.Bool(TEXT("UserLayer"))?EGamePlatformSettingLayer::User:EGamePlatformSettingLayer::Session;
        const bool UserWrite=(A.Bool(TEXT("UserLayer")) && (Op==TEXT("SetNumber") || Op==TEXT("SetInteger") || Op==TEXT("SetBoolean") || Op==TEXT("ResetCategory"))) || Op==TEXT("Save");
        if(UserWrite)
        {
            FString Key;
            if(!bOwnUserContext || !S->TryGetUserContextKey(Key) || Key!=OwnUserKey || S->GetSnapshot().bLoading)
            {Done(Context.Token,ManualError(TEXT("Demo.SettingsOwnUserRequired"),TEXT("用户写入/Save必须先成功进入本RunId用户上下文并等真实Reload完成。")));return;}
            bUserTouched=true;
        }
        if(Op==TEXT("SetNumber"))R=S->SetValue(Provider->Key(TEXT("Number")),Layer,FGamePlatformSettingValue::MakeNumber(A.Number(TEXT("NumberValue"))));
        else if(Op==TEXT("SetInteger"))R=S->SetValue(Provider->Key(TEXT("Integer")),Layer,FGamePlatformSettingValue::MakeInteger(A.Int(TEXT("IntegerValue"))));
        else if(Op==TEXT("SetBoolean"))R=S->SetValue(Provider->Key(TEXT("Boolean")),Layer,FGamePlatformSettingValue::MakeBool(A.Bool(TEXT("BooleanValue"))));
        else if(Op==TEXT("Apply"))R=S->Apply();
        else if(Op==TEXT("ResetCategory"))R=S->ResetCategory(Provider->Category(),Layer);
        else if(Op==TEXT("Reload"))R=S->Reload();
        else if(Op==TEXT("SwitchOwnUser"))
        {
            FString Key;
            if(!bOriginalContextCaptured || !S->TryGetUserContextKey(Key) || Key!=(bOwnUserContext?OwnUserKey:OriginalUserKey))
                R=ManualError(TEXT("Demo.SettingsExternalContext"),TEXT("原用户上下文已变化，不能覆盖。"));
            else {R=S->SwitchUserContext(OwnUserKey);if(R.IsSuccess())bOwnUserContext=true;}
        }
        else if(Op==TEXT("Save"))R=S->Save();
        else if(Op==TEXT("StageDevice"))
        {
            auto Candidate=D->GetSnapshot().Current;
            const int64 Width=A.Int(TEXT("Width")),Height=A.Int(TEXT("Height"));
            if(Width<1 || Width>16384 || Height<1 || Height>16384){Done(Context.Token,ManualError(TEXT("Demo.DeviceResolution"),TEXT("分辨率参数须1..16384，仍由原生复核。")));return;}
            Candidate.Resolution=FIntPoint(static_cast<int32>(Width),static_cast<int32>(Height));Candidate.FrameRateLimit=static_cast<float>(A.Number(TEXT("FPS")));
            Candidate.bVSyncEnabled=A.Bool(TEXT("VSync"));R=D->StageDeviceSettings(Candidate);if(R.IsSuccess())bDeviceTouched=true;
        }
        else if(Op==TEXT("PreviewDevice"))R=D->ApplyStagedSettings(EGamePlatformDeviceSettingsApplyMode::Preview);
        else if(Op==TEXT("ConfirmPreview"))R=D->ConfirmPreview();
        else if(Op==TEXT("CancelPreview"))R=D->CancelPreview();
        else if(Op==TEXT("DiscardStaged"))R=D->DiscardStagedSettings();
        else if(Op==TEXT("Inspect"))
        {
            FGamePlatformSettingDescriptor Desc;
            const bool Found=S->GetDescriptor(Provider->Key(TEXT("Number")),Desc);
            R=ManualObserved(Found?TEXT("真实描述符已发现，三种键归本RunId Provider。"):TEXT("描述符尚未发现，等待Reload真实事件。"));
        }
        else R=ManualError(TEXT("Demo.SettingsOperation"),TEXT("设置操作不在白名单。"));
        R.Message+=FString::Printf(TEXT("；Revision=%lld Loading=%d Dirty=%d SaveInFlight=%d Preview=%d Events=%llu"),
            View->Snapshot.Revision,View->Snapshot.bLoading,View->Snapshot.bDirty,View->Snapshot.bSaveInFlight,View->Device.bPreviewActive,View->Events);
        Done(Context.Token,R);
    }
    TUniquePtr<FManualSettingsProvider> Provider;
    TSharedRef<FManualSettingsView> View;
    FGamePlatformDeviceSettings OriginalDevice;
    FGamePlatformSettingsRuntimeSubscription SettingsSub;
    FGamePlatformDeviceSettingsSubscription DeviceSub;
    bool bProviderRegistered=false,bDeviceTouched=false,bOwnUserContext=false,bOriginalContextCaptured=false,bUserTouched=false;
    FString OriginalUserKey,OwnUserKey; // 只在内存持有，绝不投影或日志。
    TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> HostLease;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformSettingsScenario(UWorld& W,FName F)
{return MakeUnique<FManualSettingsScenario>(W,F);}
}

