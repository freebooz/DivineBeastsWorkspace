#include "Services/GamePlatformWorldServices.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
/** 任务没有世界资源所有权；Data租约和流送源由世界服务持有。Release只撤销本次绑定。 */
class FWorldReadinessTask final : public IGamePlatformLoadingTask
{
public:
    FGamePlatformResult Start(UGameInstance& GI,const FGamePlatformLoadingTaskSpec&) override
    {
        Instance=&GI;World=GI.GetWorld();
        auto* Service=World.IsValid()?IGamePlatformWorldService::Get(*World.Get()):nullptr;
        if(!Service)return FGamePlatformResult::Failure(TEXT("WorldServiceMissing"),TEXT("当前实例没有运行世界服务"));
        Generation=Service->GetReadiness().Context.ContextGeneration;
        return FGamePlatformResult::Success();
    }
    FGamePlatformLoadingTaskUpdate Poll() override
    {
        auto* Service=GetService();
        if(!Service)return {EGamePlatformLoadingTaskUpdate::Failed,0,TEXT("WorldContextExpired")};
        const auto Snapshot=Service->GetReadiness();
        if(Snapshot.Context.ContextGeneration!=Generation)return {EGamePlatformLoadingTaskUpdate::Failed,0,TEXT("WorldGenerationMismatch")};
        if(Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Ready)return {EGamePlatformLoadingTaskUpdate::Succeeded,1,NAME_None};
        if(Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed||Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated)
            return {EGamePlatformLoadingTaskUpdate::Failed,0,Snapshot.Context.Result.Code};
        return {};
    }
    bool IsReadyToUse() const override
    {
        auto* Service=GetService();if(!Service)return false;
        const auto Snapshot=Service->GetReadiness();
        return Snapshot.Context.ContextGeneration==Generation&&Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Ready;
    }
    void Release() override {World.Reset();Instance.Reset();Generation.Invalidate();}
private:
    IGamePlatformWorldService* GetService() const
    {
        return Instance.IsValid()&&World.IsValid()&&Instance->GetWorld()==World.Get()&&!World->bIsTearingDown?
            IGamePlatformWorldService::Get(*World.Get()):nullptr;
    }
    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<UWorld> World;
    FGuid Generation;
};
TUniquePtr<IGamePlatformLoadingTask> GamePlatformWorldServices::CreateReadinessTask(){return MakeUnique<FWorldReadinessTask>();}
