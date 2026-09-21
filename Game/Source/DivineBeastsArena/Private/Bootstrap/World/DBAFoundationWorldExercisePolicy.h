#pragma once

/** 项目专用纯决策：只消费UE适配层的已观察事实；不调用Travel，也不制造世界/就绪证据。 */
namespace DBAFoundationWorldExercise
{
    enum class EStage { Disabled, WaitFirstReady, ReleaseFirst, WaitBootstrap, WaitSandbox, WaitSecondReady, Completed, Failed };
    enum class EAction { None, ReleaseOwned, OpenBootstrap, OpenSandbox, Complete, Reject };
    struct FFacts
    {
        bool bDedicated=false;
        bool bNetworkPeer=false;
        bool bSameInstance=false;
        bool bBegunPlay=false;
        bool bSandbox=false;
        bool bBootstrap=false;
        bool bOperationMatched=false;
        bool bReady=false;
        bool bOwnedReleased=false;
        bool bFreshGeneration=false;
        bool bOldGenerationRejected=false;
        bool bTimedOut=false;
    };
    class FPolicy
    {
    public:
        /** 参数必须来自显式开发开关；默认构造不允许任何Travel动作。 */
        explicit FPolicy(bool bEnabled=false):Current(bEnabled?EStage::WaitFirstReady:EStage::Disabled){}
        EStage Stage() const {return Current;}
        /** 每次采样最多推进一个阶段；OpenLevel返回不能替代下一次实际世界观察。 */
        EAction Step(const FFacts& Facts)
        {
            if(Current==EStage::Disabled||Current==EStage::Completed||Current==EStage::Failed)return EAction::None;
            if(Facts.bDedicated||Facts.bNetworkPeer||Facts.bTimedOut){Current=EStage::Failed;return EAction::Reject;}
            const bool bWorld=Facts.bSameInstance&&Facts.bBegunPlay;
            if(Current==EStage::WaitFirstReady&&bWorld&&Facts.bSandbox&&Facts.bReady)
            {Current=EStage::ReleaseFirst;return EAction::ReleaseOwned;}
            if(Current==EStage::ReleaseFirst&&bWorld&&Facts.bSandbox&&Facts.bOwnedReleased)
            {Current=EStage::WaitBootstrap;return EAction::OpenBootstrap;}
            if(Current==EStage::WaitBootstrap&&bWorld&&Facts.bBootstrap&&Facts.bOperationMatched)
            {Current=EStage::WaitSandbox;return EAction::OpenSandbox;}
            if(Current==EStage::WaitSandbox&&bWorld&&Facts.bSandbox&&Facts.bOperationMatched)
            {Current=EStage::WaitSecondReady;return EAction::None;}
            if(Current==EStage::WaitSecondReady&&bWorld&&Facts.bSandbox&&Facts.bOperationMatched&&Facts.bReady)
            {
                if(!Facts.bFreshGeneration){Current=EStage::Failed;return EAction::Reject;}
                if(Facts.bOldGenerationRejected){Current=EStage::Completed;return EAction::Complete;}
            }
            return EAction::None;
        }
    private:
        EStage Current;
    };
}
