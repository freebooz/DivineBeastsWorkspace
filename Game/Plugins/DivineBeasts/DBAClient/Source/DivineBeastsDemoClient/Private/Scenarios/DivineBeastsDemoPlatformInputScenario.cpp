// 真实LocalPlayer输入服务人工控制；只使用页所属玩家，精确恢复本案例拥有的租约和偏好。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Interfaces/IGamePlatformInputService.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Types/GamePlatformId.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
namespace DivineBeasts::Demo
{
struct FManualInputView
{
    TWeakObjectPtr<UWorld> World;
    FGamePlatformInputSnapshot Snapshot;
    FDivineBeastsDemoRunToken Token;
    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> Publish;
    bool bAlive=true;
    uint64 Events=0;
};
class FManualInputScenario final : public FPlatformManualBase
{
public:
    FManualInputScenario(UWorld& W,FName F):FPlatformManualBase(W,F),View(MakeShared<FManualInputView>()){}
    ~FManualInputScenario() override{CleanupService();}
private:
    IGamePlatformInputService* Service() const{return Player.IsValid()?IGamePlatformInputService::Get(*Player):nullptr;}
    FGamePlatformResult PrepareService() override
    {
        const auto* Widget=Cast<UUserWidget>(Context.SampleWidget.Get());
        Player=Widget?Widget->GetOwningLocalPlayer():nullptr;
        auto* I=Service();
        if(!I || !Widget || Widget->GetWorld()!=SourceWorld.Get())
            return ManualError(TEXT("Demo.InputLocalPlayerHost"),TEXT("需要真实页Widget所属LocalPlayer；不使用第零玩家或替代玩家。"));
        View->World=SourceWorld;View->Token=Context.Token;View->Publish=Context.PublishObservation;View->Snapshot=I->GetInputSnapshot();OriginalAccessibility=I->GetAccessibilitySettings();
        OriginalDevice=View->Snapshot.ActiveDeviceFamily;
        TWeakPtr<FManualInputView> Weak(View);
        Subscription=I->SubscribeInputState(Context.SampleWidget,[Weak](const auto& Snapshot)
        {auto V=Weak.Pin();if(!V || !V->bAlive || !V->World.IsValid() || V->World->bIsTearingDown)return;V->Snapshot=Snapshot;++V->Events;
            if(V->Publish)V->Publish(V->Token,FString::Printf(TEXT("Input Profile=%d Binding=%d Blocked=%d Device=%d TouchSources=%d Error=%s"),
                Snapshot.bProfilePrepared,Snapshot.bBindingsReady,Snapshot.BlockedChannels,int32(Snapshot.ActiveDeviceFamily),Snapshot.TouchSourceCount,*Snapshot.Result.Code.ToString()));});
        return Subscription.IsValid()?FGamePlatformResult::Success():ManualError(TEXT("Demo.InputSubscribe"),TEXT("输入状态订阅未被接纳。"));
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;auto* I=Service();
        const bool Owned=Subscription.IsValid() || Touch.IsValid() || Binding.IsValid() || Block.IsValid() || InputContext.IsValid() || Profile.IsValid();
        if(!I)return Owned?ManualError(TEXT("Demo.InputCleanupMissing"),TEXT("输入服务失效；保留完整自有句柄诊断。")):FGamePlatformResult::Success();
        FGamePlatformResult R=FGamePlatformResult::Success();
        auto Keep=[&R](const FGamePlatformResult& X){if(R.IsSuccess() && !X.IsSuccess())R=X;};
        if(Subscription.IsValid()){if(!I->UnsubscribeInputState(Subscription))Keep(ManualError(TEXT("Demo.InputUnsubscribe"),TEXT("撤销输入状态订阅失败。")));else Subscription={};}
        if(Touch.IsValid()){auto X=I->EndTouchInput(Touch);Keep(X);if(X.IsSuccess())Touch={};}
        if(Binding.IsValid()){auto X=I->UnbindInputReceiver(Binding);Keep(X);if(X.IsSuccess())Binding={};}
        if(InputContext.IsValid()){auto X=I->ReleaseInputContext(InputContext);Keep(X);if(X.IsSuccess())InputContext={};}
        if(Block.IsValid()){auto X=I->ReleaseInputBlock(Block);Keep(X);if(X.IsSuccess())Block={};}
        if(bAccessibilityTouched){auto X=I->SetAccessibilitySettings(OriginalAccessibility);Keep(X);if(X.IsSuccess())bAccessibilityTouched=false;}
        if(bDeviceTouched){if(I->GetInputSnapshot().ActiveDeviceFamily!=LastInjectedDevice)bDeviceTouched=false;
            else {auto X=I->NotifyInputDeviceActivity(OriginalDevice);Keep(X);if(X.IsSuccess())bDeviceTouched=false;}}
        // 只允许在本适配自己准备的Profile中重绑；释放Profile撤销自有原生行；显式保存仅留于本次专属UserDir，不删除其证据。
        if(Profile.IsValid()){auto X=I->ReleaseInputProfile(Profile);Keep(X);if(X.IsSuccess())Profile={};}
        return R;
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        auto* I=Service();if(!I){Done(Context.Token,ManualError(TEXT("Demo.InputMissing"),TEXT("输入服务失效。")));return;}
        FGamePlatformResult R;
        if(Op==TEXT("PrepareProfile"))
        {
            if(Profile.IsValid() || I->GetInputSnapshot().ProfileGeneration!=0)
            {Done(Context.Token,ManualError(TEXT("Demo.InputProfileAlreadyActive"),TEXT("当前已有Profile，不替换项目/其他调用者配置；请用独立输入测试玩家。")));return;}
            FGamePlatformId Id;
            const int64 Version=A.Int(TEXT("LogicalVersion"));
            if(Version<1 || Version>MAX_int32 || !FGamePlatformId::TryCreate(A.Id(TEXT("Namespace")).ToString(),A.Id(TEXT("Name")).ToString(),static_cast<int32>(Version),Id))
            {Done(Context.Token,ManualError(TEXT("Demo.InputProfileIdentity"),TEXT("输入Profile身份非法。")));return;}
            Profile=I->PrepareInputProfile(FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(),FName(*Id.ToString())),
                TEXT("ManualInput_")+Context.Token.RunId.ToString(EGuidFormats::Digits),R);
            if(R.IsSuccess())R.Message=TEXT("真实Profile加载已受理，准备结果由输入状态事件报告。");
        }
        else if(Op==TEXT("AcquireContext"))
        {
            if(InputContext.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.InputContextOwned"),TEXT("先释放自有上下文。")));return;}
            const int64 Priority=A.Int(TEXT("Priority"));
            if(Priority<0 || Priority>100){Done(Context.Token,ManualError(TEXT("Demo.InputPriority"),TEXT("上下文优先级必须0..100。")));return;}
            InputContext=I->AcquireInputContext(A.Id(TEXT("ContextName")),static_cast<int32>(Priority),Context.SampleWidget,R);
        }
        else if(Op==TEXT("ReleaseContext")){R=I->ReleaseInputContext(InputContext);if(R.IsSuccess())InputContext={};}
        else if(Op==TEXT("BindReceiver"))
        {
            const auto* Widget=Cast<UUserWidget>(Context.SampleWidget.Get());
            auto* PC=Widget?Widget->GetOwningPlayer():nullptr;auto* Component=PC?Cast<UEnhancedInputComponent>(PC->InputComponent):nullptr;
            if(!Component || I->GetInputSnapshot().OwnedBindingCount!=0)
            {Done(Context.Token,ManualError(TEXT("Demo.InputReceiverHost"),TEXT("需要本页控制器的独立增强输入组件，不能接管已有绑定。")));return;}
            Binding=I->BindInputReceiver(*Component,R);
        }
        else if(Op==TEXT("UnbindReceiver")){R=I->UnbindInputReceiver(Binding);if(R.IsSuccess())Binding={};}
        else if(Op==TEXT("AcquireBlock"))
        {
            const int64 Mask=A.Int(TEXT("ChannelMask"));
            if(Block.IsValid() || Mask<1 || Mask>31){Done(Context.Token,ManualError(TEXT("Demo.InputBlockMask"),TEXT("阻断掩码1..31且仅持有一份自有Block。")));return;}
            Block=I->AcquireInputBlock(static_cast<uint8>(Mask),TEXT("ManualFixture"),Context.SampleWidget,R);
        }
        else if(Op==TEXT("ReleaseBlock")){R=I->ReleaseInputBlock(Block);if(R.IsSuccess())Block={};}
        else if(Op==TEXT("SetAccessibility"))
        {
            auto Settings=OriginalAccessibility;Settings.LookSensitivityMultiplier=A.Number(TEXT("Sensitivity"));Settings.bInvertLookY=A.Bool(TEXT("InvertY"));
            R=I->SetAccessibilitySettings(Settings);if(R.IsSuccess())bAccessibilityTouched=true;
        }
        else if(Op==TEXT("NotifyDevice"))
        {
            if(OriginalDevice==EGamePlatformInputDeviceFamily::Unknown){Done(Context.Token,ManualError(TEXT("Demo.InputOriginalDeviceUnknown"),TEXT("先由真实视口产生一次设备活动；当前Unknown没有公开恢复入口，不能先修改再伪造恢复。")));return;}
            const int64 Device=A.Int(TEXT("Device"));
            if(Device<1 || Device>3){Done(Context.Token,ManualError(TEXT("Demo.InputDevice"),TEXT("设备族只能1键鼠/2手柄/3触控。")));return;}
            R=I->NotifyInputDeviceActivity(static_cast<EGamePlatformInputDeviceFamily>(Device));if(R.IsSuccess()){bDeviceTouched=true;LastInjectedDevice=static_cast<EGamePlatformInputDeviceFamily>(Device);}
        }
        else if(Op==TEXT("PreviewRebind") || Op==TEXT("ApplyRebind") || Op==TEXT("ResetMappings"))
        {
            if(Op!=TEXT("PreviewRebind") && !Profile.IsValid())
            {Done(Context.Token,ManualError(TEXT("Demo.InputRebindOwnership"),TEXT("修改映射只允许本案例准备的独立Profile。")));return;}
            const FKey Keys[]={EKeys::K,EKeys::L,EKeys::SpaceBar,EKeys::Gamepad_FaceButton_Bottom};
            const int64 Choice=A.Int(TEXT("KeyChoice")),Slot=A.Int(TEXT("KeySlot"));
            if(Choice<0 || Choice>3 || Slot<0 || Slot>7){Done(Context.Token,ManualError(TEXT("Demo.InputKeyChoice"),TEXT("KeyChoice须0..3，KeySlot须0..7并由原生复核。")));return;}
            if(Op==TEXT("PreviewRebind"))R=I->PreviewRebind(A.Id(TEXT("MappingRow")),static_cast<int32>(Slot),Keys[Choice]).Result;
            else if(Op==TEXT("ApplyRebind"))R=I->ApplyRebind(A.Id(TEXT("MappingRow")),static_cast<int32>(Slot),Keys[Choice]);
            else R=I->ResetMappings(A.Id(TEXT("MappingRow")));
        }
        else if(Op==TEXT("SavePreferences"))
        {
            if(SourceWorld->WorldType!=EWorldType::PIE)
            {
                FString NativeSaved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());FPaths::NormalizeDirectoryName(NativeSaved);
                const FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("../Saved/Validation/PluginManualUI20261027")));
                const FString NativeIni=FPaths::ConvertRelativePathToFull(GGameUserSettingsIni);
                if(SourceWorld->WorldType!=EWorldType::Game || !FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture")) ||
                    !FPaths::IsUnderDirectory(NativeSaved,Allowed) || NativeSaved.Equals(Allowed,ESearchCase::IgnoreCase) ||
                    !FPaths::IsUnderDirectory(NativeIni,NativeSaved) || !Profile.IsValid())
                {Done(Context.Token,ManualError(TEXT("Demo.InputSaveValidationHost"),TEXT("保存需要Game世界、专属ManualFixture UserDir、其内的原生INI及本RunId Profile；路径仅由引擎解析。")));return;}
            }
            R=I->SaveInputPreferences();
            if(R.IsSuccess())R.Message=TEXT("已调用原生EnhancedInput SaveSettings及INI Flush；仅提交，底层void无落盘完成合同，Saved保持false。独立UserDir保留本次验证证据。");
        }
        else if(Op==TEXT("BeginTouch"))
        {
            const int64 Pointer=A.Int(TEXT("Pointer"));
            if(Pointer<0 || Pointer>9 || Touch.IsValid()){Done(Context.Token,ManualError(TEXT("Demo.InputTouchPointer"),TEXT("触点须0..9且本案例一次一个。")));return;}
            FGamePlatformInputSemanticId Id;Id.Tag=FGameplayTag::RequestGameplayTag(A.Id(TEXT("SemanticTag")),false);
            Touch=I->BeginTouchInputBySemantic(static_cast<int32>(Pointer),Id,Context.SampleWidget,R);
        }
        else if(Op==TEXT("UpdateTouch"))R=I->UpdateTouchInput(Touch,FInputActionValue(FVector2D(A.Number(TEXT("AxisX")),A.Number(TEXT("AxisY")))));
        else if(Op==TEXT("EndTouch")){R=I->EndTouchInput(Touch);if(R.IsSuccess())Touch={};}
        else if(Op==TEXT("Inspect"))R=FGamePlatformResult::Success();
        else R=ManualError(TEXT("Demo.InputOperation"),TEXT("未允许的输入操作；应用焦点只由真实视口桥报告。"));
        const auto& S=View->Snapshot;
        R.Message+=FString::Printf(TEXT("；Profile=%d Bindings=%d Mappings=%d Blocks=%d Touch=%d Events=%llu SaveSubmitted=%d Saved=%d"),
            S.bProfilePrepared,S.bBindingsReady,S.bMappingsApplied,S.BlockedChannels,S.TouchSourceCount,View->Events,S.bPreferencesSaveSubmitted,S.bPreferencesSaved);
        Done(Context.Token,R);
    }
    TWeakObjectPtr<ULocalPlayer> Player;
    TSharedRef<FManualInputView> View;
    FGamePlatformInputAccessibilitySettings OriginalAccessibility;
    EGamePlatformInputDeviceFamily LastInjectedDevice=EGamePlatformInputDeviceFamily::Unknown;
    EGamePlatformInputDeviceFamily OriginalDevice=EGamePlatformInputDeviceFamily::Unknown;
    bool bAccessibilityTouched=false,bDeviceTouched=false;
    FGamePlatformInputStateSubscription Subscription;
    FGamePlatformInputProfileHandle Profile;
    FGamePlatformInputContextHandle InputContext;
    FGamePlatformInputBlockHandle Block;
    FGamePlatformInputBindingHandle Binding;
    FGamePlatformInputTouchHandle Touch;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformInputScenario(UWorld& W,FName F)
{return MakeUnique<FManualInputScenario>(W,F);}
}

