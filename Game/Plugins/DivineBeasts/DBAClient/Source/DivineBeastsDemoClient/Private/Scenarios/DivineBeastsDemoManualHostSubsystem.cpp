// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

#include "Misc/Parse.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Misc/App.h"
#endif

bool UDivineBeastsDemoManualHostSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{return Super::ShouldCreateSubsystem(Outer) && FDivineBeastsPluginDemoMode::IsEnabled();}
FGamePlatformResult UDivineBeastsDemoManualHostSubsystem::Acquire(const FDivineBeastsDemoRunToken& T,UWorld& W)
{
    check(IsInGameThread());
    if(!T.IsValid() || W.GetGameInstance()!=GetGameInstance() || W.WorldType!=EWorldType::Game ||
        W.GetNetMode()!=NM_Standalone || !FDivineBeastsPluginDemoMode::IsEnabled() ||
        !FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualFixture")))
        return FGamePlatformResult::Failure(TEXT("Demo.DedicatedServiceHostRequired"),TEXT("需要专属Standalone Demo Host实际GI及显式夹具标志。"));
    if(Owner.IsValid() && (!(Owner==T) || Source.Get()!=&W))
        return FGamePlatformResult::Failure(TEXT("Demo.ServiceHostOwned"),TEXT("本GI已有另一运行拥有平台服务，不能接管。"));
    Owner=T;Source=&W;return FGamePlatformResult::Success();
}

bool UDivineBeastsDemoManualHostSubsystem::IsEditorFeature(FName F){
    static const TArray<FName> Allowed={TEXT("GamePlatformDeveloperTools.asset_audit"),TEXT("GamePlatformDeveloperTools.validation"),TEXT("GamePlatformDeveloperTools.human_review"),TEXT("GamePlatformDeveloperTools.performance"),TEXT("GamePlatformPCG.authoring"),TEXT("GamePlatformPCG.spatial_bake"),TEXT("GamePlatformSurface.editor_wind"),TEXT("BlueprintAutoLayout.whole"),TEXT("BlueprintAutoLayout.selected"),TEXT("BlueprintAutoLayout.group"),TEXT("BlueprintAutoLayout.route"),TEXT("BlueprintAutoLayout.settings"),TEXT("GlooPrint.format"),TEXT("GlooPrint.wires"),TEXT("GlooPrint.settings"),TEXT("GlooPrint.cancelundo")};
    return Allowed.Contains(F);
}
bool UDivineBeastsDemoManualHostSubsystem::OwnsEditorFixture(const FDivineBeastsDemoRunToken& T)const {
#if WITH_EDITOR
    auto* R=IGamePlatformEditorResources::Get();return Owns(T)&&R&&R->Owns(EditorLease);
#else
    return false;
#endif
}
FGamePlatformResult UDivineBeastsDemoManualHostSubsystem::ValidateEditorFixtureSource(const FDivineBeastsDemoRunToken& T,UWorld& W,FName F)const{
    check(IsInGameThread());
#if WITH_EDITOR
    FString Map;const TCHAR* Args=FCommandLine::Get();
    int32 PIECount=0;bool bEngineOwns=false;
    if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&C.World()){++PIECount;if(C.World()==&W&&C.OwningGameInstance==GetGameInstance())bEngineOwns=true;}
    const FString Package=W.GetOutermost()->GetName();const FString CleanPackage=UWorld::RemovePIEPrefix(Package);
    const bool bExact=T.IsValid()&&IsEditorFeature(F)&&GEditor&&GIsEditor&&FString(FApp::GetProjectName())==TEXT("DivineBeastsArena")&&
        FDivineBeastsPluginDemoMode::IsHubPageEnabled()&&FParse::Param(Args,TEXT("GamePlatformManualFixture"))&&FParse::Param(Args,TEXT("GamePlatformManualEditorFixture"))&&
        !FParse::Param(Args,TEXT("game"))&&!FParse::Param(Args,TEXT("GameplayManualNative"))&&!FParse::Param(Args,TEXT("WorldManualNative"))&&!FParse::Param(Args,TEXT("FoundationWorld"))&&!FParse::Param(Args,TEXT("DBAManualWorldHost"))&&!FParse::Param(Args,TEXT("DBAManualGameplayHost"))&&!FParse::Param(Args,TEXT("FoundationGameplayOffline"))&&
        W.WorldType==EWorldType::PIE&&W.GetNetMode()==NM_Standalone&&!W.bIsTearingDown&&GEditor->PlayWorld==&W&&PIECount==1&&bEngineOwns&&W.GetGameInstance()==GetGameInstance()&&
        GetGameInstance()->GetLocalPlayers().Num()==1&&GetGameInstance()->GetFirstGamePlayer()&&GetGameInstance()->GetFirstGamePlayer()->GetWorld()==&W&&
        CleanPackage==FDivineBeastsPluginDemoMode::GetHubMapPackage();
    if(!bExact)return FGamePlatformResult::Failure(TEXT("Demo.EditorFixtureHostRequired"),TEXT("要求本工程精确Hub、显式Editor夹具、引擎单人Standalone网络PIE/GI/LP和准确Editor能力。"));
    return FGamePlatformResult::Success();
#else
    return FGamePlatformResult::Unsupported(TEXT("Demo.EditorHostRequired"),TEXT("普通Client没有Editor桥。"));
#endif
}
FGamePlatformResult UDivineBeastsDemoManualHostSubsystem::AcquireEditorFixture(const FDivineBeastsDemoRunToken& T,UWorld& W,FName F){
    check(IsInGameThread());const auto SourceResult=ValidateEditorFixtureSource(T,W,F);if(!SourceResult.IsSuccess())return SourceResult;
#if WITH_EDITOR
    if(Owner.IsValid())return OwnsEditorFixture(T)&&Source.Get()==&W?FGamePlatformResult::Success():FGamePlatformResult::Failure(TEXT("Demo.ServiceHostOwned"),TEXT("本GI已有另一运行，不能接管。"));
    auto* Resources=IGamePlatformEditorResources::Get();FString Error;
    if(!Resources||!Resources->Acquire(*GetGameInstance(),W,T.RunId,T.WorldId,T.Generation,EditorLease,Error))return FGamePlatformResult::Failure(TEXT("Demo.EditorResourcesOwned"),Error.IsEmpty()?TEXT("Editor资源模块未加载。"):Error);
    Owner=T;Source=&W;return FGamePlatformResult::Success();
#else
    return FGamePlatformResult::Unsupported(TEXT("Demo.EditorHostRequired"),TEXT("普通Client没有Editor桥。"));
#endif
}
FGamePlatformResult UDivineBeastsDemoManualHostSubsystem::Release(const FDivineBeastsDemoRunToken& T)
{
    check(IsInGameThread());
    if(!Owner.IsValid())return FGamePlatformResult::Success();
    if(!(Owner==T))return FGamePlatformResult::Failure(TEXT("Demo.ServiceHostTokenMismatch"),TEXT("不能释放另一完整RunToken的宿主租约。"));

#if WITH_EDITOR
    if(EditorLease.IsValid()){auto* R=IGamePlatformEditorResources::Get();FString Error;if(!R||!R->Release(EditorLease,Error))return FGamePlatformResult::Failure(TEXT("Demo.EditorReleasePending"),Error);EditorLease={};}
#endif
    Owner={};Source.Reset();return FGamePlatformResult::Success();

}
