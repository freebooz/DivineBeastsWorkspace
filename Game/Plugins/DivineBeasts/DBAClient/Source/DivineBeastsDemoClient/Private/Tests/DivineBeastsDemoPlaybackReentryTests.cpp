#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/DivineBeastsDemoPresentationLiveUtilities.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Interfaces/IGamePlatformSFXService.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBALivePlaybackStopReentry,"DivineBeasts.Demo.Presentation.LivePlaybackStopReentry",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBALivePlaybackStopReentry::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;using namespace DivineBeasts::Demo::Tests;
    for(bool bVFX:{true,false})for(int32 Count:{1,3})
    {
        auto S=MakeState(*this,bVFX?FName(TEXT("GamePlatformVFX.ten_behaviors")):FName(TEXT("GamePlatformSFX.spaces")));if(!S)return false;
        auto* V=IGamePlatformVFXService::Get(S->World.Get());auto* A=IGamePlatformSFXService::Get(S->World.Get());
        if(!V||!A){AddError(TEXT("真实VFX/SFX服务不可用。"));S->Cleanup();return false;}
        for(int32 I=0;I<Count;++I)
        {
            if(bVFX){FGamePlatformVFXRequest R;R.RequestId=FGuid::NewGuid();R.DefinitionId=TEXT("presentation.dba.development.magic.frostbolt.impact@1");const auto H=V->Play(R);if(!H.Handle.IsValid()){AddError(TEXT("真实VFX未提供受理句柄。"));S->Cleanup();return false;}S->VFXHandles.Add(H.Handle);}
            else{FGamePlatformSFXRequest R;R.RequestId=FGuid::NewGuid();R.DefinitionId=TEXT("presentation.dba.audio.ui.click@1");const auto H=A->Play(R);if(!H.Handle.IsValid()){AddError(TEXT("真实SFX未提供受理句柄。"));S->Cleanup();return false;}S->SFXHandles.Add(H.Handle);}
        }
        const auto VH=S->VFXHandles;const auto AH=S->SFXHandles;int32 Stops=0;FGamePlatformResult Nested;
        const TWeakPtr<FPresentationManualState> Weak=S;
        S->Context.PublishObservation=[&Stops,&Nested,Weak](const auto&,const FString&){if(Stops++==0)if(auto Self=Weak.Pin())Nested=Self->Cleanup();};
        const auto Reset=S->ResetPlayback();
        TestEqual(TEXT("真实同步完成只引发一次Stop尝试"),Stops,1);TestTrue(TEXT("重入Stop实际清理成功"),Nested.IsSuccess());
        TestTrue(TEXT("旧Reset栈以取消收敛"),Reset.Status==EGamePlatformResultStatus::Cancelled);
        TestTrue(TEXT("完整句柄所有权全部撤销"),S->VFXHandles.IsEmpty()&&S->SFXHandles.IsEmpty());
        for(const auto& H:VH)TestFalse(TEXT("真实VFX没有残留活动句柄"),V->IsActive(H));
        for(const auto& H:AH)TestFalse(TEXT("真实SFX没有残留活动句柄"),A->IsActive(H));
        TestFalse(TEXT("真实宿主已释放"),S->Host->Owns(S->Context.Token));S->Context.PublishObservation={};S->Cleanup();
    }
    return true;
}
#endif
