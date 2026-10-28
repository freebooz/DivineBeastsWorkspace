#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
FGamePlatformResult UDivineBeastsDemoManualHostSubsystem::Release(const FDivineBeastsDemoRunToken& T)
{
    check(IsInGameThread());
    if(!Owner.IsValid())return FGamePlatformResult::Success();
    if(!(Owner==T))return FGamePlatformResult::Failure(TEXT("Demo.ServiceHostTokenMismatch"),TEXT("不能释放另一完整RunToken的宿主租约。"));
    Owner={};Source.Reset();return FGamePlatformResult::Success();
}

