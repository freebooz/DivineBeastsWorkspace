// 本文件在UE模块中为空翻译单元；native入口仅由旁边CMake显式启用，不能冒充UE测试。
#if defined(DBA_WORLD_BOOTSTRAP_NATIVE_TEST) && DBA_WORLD_BOOTSTRAP_NATIVE_TEST
#include "../DBAFoundationWorldExercisePolicy.h"
#include <iostream>
#include <stdexcept>
using namespace DBAFoundationWorldExercise;
static void Check(bool Value,const char* Message){if(!Value)throw std::runtime_error(Message);}
int main()
{
    int Cases=0;
    try
    {
        FFacts Facts;Facts.bSameInstance=true;Facts.bBegunPlay=true;Facts.bSandbox=true;Facts.bReady=true;
        FPolicy Disabled(false);Check(Disabled.Step(Facts)==EAction::None,"默认不Travel");++Cases;
        FPolicy Dedicated(true);Facts.bDedicated=true;Check(Dedicated.Step(Facts)==EAction::Reject,"Dedicated禁止玩家路径");Facts.bDedicated=false;++Cases;
        FPolicy Network(true);Facts.bNetworkPeer=true;Check(Network.Step(Facts)==EAction::Reject,"没有Session不允许网络例外");Facts.bNetworkPeer=false;++Cases;
        FPolicy Policy(true);Facts.bSameInstance=false;Check(Policy.Step(Facts)==EAction::None,"其他实例不能满足就绪");Facts.bSameInstance=true;++Cases;
        Facts.bBegunPlay=false;Check(Policy.Step(Facts)==EAction::None,"没有BeginPlay不能Travel");Facts.bBegunPlay=true;++Cases;
        Check(Policy.Step(Facts)==EAction::ReleaseOwned,"首轮Ready先清理");++Cases;
        Check(Policy.Step(Facts)==EAction::None,"Busy时不能打开地图");++Cases;
        Facts.bOwnedReleased=true;Facts.bSameInstance=false;Check(Policy.Step(Facts)==EAction::None,"清理后世界切走也不得发旧Travel");Facts.bSameInstance=true;++Cases;
        Check(Policy.Step(Facts)==EAction::OpenBootstrap,"清理完成才离开Sandbox");++Cases;
        Facts.bSandbox=false;Facts.bBootstrap=true;Check(Policy.Step(Facts)==EAction::None,"错误operation不能推进");++Cases;
        Facts.bOperationMatched=true;Check(Policy.Step(Facts)==EAction::OpenSandbox,"实际Bootstrap才返回Sandbox");++Cases;
        Check(Policy.Step(Facts)==EAction::None&&Policy.Stage()==EStage::WaitSandbox,"OpenLevel调用不等于已到达Sandbox");++Cases;
        Facts.bBootstrap=false;Facts.bSandbox=true;Facts.bReady=false;
        Check(Policy.Step(Facts)==EAction::None&&Policy.Stage()==EStage::WaitSecondReady,"实际新Sandbox等待新屏障");++Cases;
        Facts.bReady=true;Facts.bFreshGeneration=true;
        Check(Policy.Step(Facts)==EAction::None,"必须实际检验旧代次拒绝");Facts.bOldGenerationRejected=true;++Cases;
        Check(Policy.Step(Facts)==EAction::Complete,"新代次Ready才完成");++Cases;
        Check(Policy.Step(Facts)==EAction::None,"完成只发一次");++Cases;
        FPolicy Expired(true);Facts.bTimedOut=true;Check(Expired.Step(Facts)==EAction::Reject,"截止超时真实失败");++Cases;
        FPolicy Stale(true);Facts={};Facts.bSameInstance=true;Facts.bBegunPlay=true;Facts.bSandbox=true;Facts.bReady=true;
        Stale.Step(Facts);Facts.bOwnedReleased=true;Stale.Step(Facts);Facts.bSandbox=false;Facts.bBootstrap=true;Facts.bOperationMatched=true;
        Stale.Step(Facts);Facts.bBootstrap=false;Facts.bSandbox=true;Stale.Step(Facts);Facts.bOldGenerationRejected=true;
        Check(Stale.Step(Facts)==EAction::Reject,"旧generation不得用于第二轮Ready");++Cases;
        std::cout<<"Passed "<<Cases<<" policy checks; UE/network not executed\n";return 0;
    }
    catch(const std::exception& Error){std::cerr<<"Failed after "<<Cases<<": "<<Error.what()<<'\n';return 1;}
}
#endif
