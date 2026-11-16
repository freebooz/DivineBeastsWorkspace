// 开发私有基础：不可重绑定的来源、白名单参数、同世界停止。
#pragma once
#include "Scenarios/DivineBeastsDemoPlatformServiceScenarios.h"
#include "Engine/World.h"
namespace DivineBeasts::Demo
{
using FManualValues = TArray<FDivineBeastsDemoParameterValue>;
inline FGamePlatformResult ManualError(const TCHAR* Code, const TCHAR* Message)
{ return FGamePlatformResult::Failure(FName(Code), Message); }
inline FGamePlatformResult ManualObserved(FString Text)
{ auto R=FGamePlatformResult::Success(); R.Message=MoveTemp(Text); return R; }
/** 参数按冻结作者定义解码；不反射查找，不解析控制台文本。 */
class FManualArguments
{
public:
    FManualArguments(const FDivineBeastsDemoScenarioContext& C,const FManualValues& V):Context(C),Values(V){}
    const FDivineBeastsDemoParameterValue* Find(FName Id,EDivineBeastsDemoParameterType Type) const
    {
        for(int32 I=0;I<Context.Scenario.Parameters.Num() && I<Values.Num();++I)
            if(Context.Scenario.Parameters[I].ParameterId==Id && Values[I].Type==Type) return &Values[I];
        return nullptr;
    }
    FName Id(FName Key) const {auto* V=Find(Key,EDivineBeastsDemoParameterType::Identifier);return V?V->IdentifierValue:NAME_None;}
    int64 Int(FName Key) const {auto* V=Find(Key,EDivineBeastsDemoParameterType::Integer);return V?V->IntegerValue:0;}
    double Number(FName Key) const {auto* V=Find(Key,EDivineBeastsDemoParameterType::Number);return V?V->NumberValue:0;}
    bool Bool(FName Key) const {auto* V=Find(Key,EDivineBeastsDemoParameterType::Boolean);return V && V->bBooleanValue;}
    FSoftObjectPath Asset(FName Key) const {auto* V=Find(Key,EDivineBeastsDemoParameterType::AssetPath);return V?V->AssetPathValue:FSoftObjectPath();}
private:
    const FDivineBeastsDemoScenarioContext& Context;
    const FManualValues& Values;
};
/** 一个适配只服务构造时世界和编译时功能；准备失败也不接受新Token覆盖原来源。 */
class FPlatformManualBase : public IDivineBeastsDemoScenarioAdapter
{
public:
    FPlatformManualBase(UWorld& W,FName F):SourceWorld(&W),Feature(F){}
    void Prepare(const FDivineBeastsDemoScenarioContext& C,FDivineBeastsDemoOperationCompletion Done) final
    {
        check(IsInGameThread());
        if(bHasSource){Done(C.Token,ManualError(TEXT("Demo.SourceAlreadyBound"),TEXT("适配已绑定一次来源，不能重绑定运行Token。")));return;}
        bHasSource=true; Context=C;
        const FName ExpectedAdapter(*(FString(TEXT("PlatformServices."))+Feature.ToString()));
        bActive=C.Token.IsValid() && C.World.Get()==SourceWorld.Get() && SourceWorld.IsValid() &&
            !SourceWorld->bIsTearingDown && C.Scenario.FeatureId==Feature && C.Scenario.AdapterId==ExpectedAdapter &&
            C.Scenario.ScenarioId==FName(*(Feature.ToString()+TEXT(".Manual"))) && ValidatePlatformManualContract(C.Scenario);
        if(!bActive){Done(Context.Token,ManualError(TEXT("Demo.PlatformServiceSource"),TEXT("世界、功能、案例或固定适配白名单不匹配。")));return;}
        const auto Result=PrepareService(); bActive=Result.IsSuccess();
        if(Context.PublishObservation)Context.PublishObservation(Context.Token,FString::Printf(TEXT("功能=%s；准备状态=%d；错误码=%s"),
            *Feature.ToString(),int32(Result.Status),*Result.Code.ToString()));
        Done(Context.Token,Result);
    }
    void Execute(FName Op,const FManualValues& Values,FDivineBeastsDemoOperationCompletion Done) final
    {
        check(IsInGameThread());
        if(!bActive || !SourceWorld.IsValid() || SourceWorld->bIsTearingDown)
        {Done(Context.Token,ManualError(TEXT("Demo.PlatformServiceInactive"),TEXT("本运行已停止或世界失效。")));return;}
        if(!Context.Scenario.Operations.Contains(Op) || Values.Num()!=Context.Scenario.Parameters.Num())
        {Done(Context.Token,ManualError(TEXT("Demo.PlatformServiceOperation"),TEXT("操作或参数数量不在冻结合同内。")));return;}
        for(int32 I=0;I<Values.Num();++I) if(Values[I].Type!=Context.Scenario.Parameters[I].DefaultValue.Type ||
            (Values[I].Type==EDivineBeastsDemoParameterType::Number && !FMath::IsFinite(Values[I].NumberValue)))
        {Done(Context.Token,ManualError(TEXT("Demo.PlatformServiceParameter"),TEXT("参数类型不匹配或数值非有限。")));return;}
        ExecuteService(Op,FManualArguments(Context,Values),MoveTemp(Done));
    }
    void Stop(FDivineBeastsDemoOperationCompletion Done) final
    {
        check(IsInGameThread());
        bActive=false; StopService(MoveTemp(Done));
    }
protected:
    virtual FGamePlatformResult PrepareService(){return FGamePlatformResult::Success();}
    virtual void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done)=0;
    virtual FGamePlatformResult CleanupService(){return FGamePlatformResult::Success();}
    /** 异步IO清理可等待真实事件；不得用固定延时伪造完成。 */
    virtual void StopService(FDivineBeastsDemoOperationCompletion Done){Done(Context.Token,CleanupService());}
    bool Alive() const {return bActive && SourceWorld.IsValid() && !SourceWorld->bIsTearingDown;}
    UGameInstance* Instance() const {return SourceWorld.IsValid()?SourceWorld->GetGameInstance():nullptr;}
    FDivineBeastsDemoScenarioContext Context;
    TWeakObjectPtr<UWorld> SourceWorld;
    const FName Feature;
    bool bActive=false;
private:
    bool bHasSource=false;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformValueScenario(UWorld&,FName);
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformPlayerFixture(UWorld&,FName);

}

