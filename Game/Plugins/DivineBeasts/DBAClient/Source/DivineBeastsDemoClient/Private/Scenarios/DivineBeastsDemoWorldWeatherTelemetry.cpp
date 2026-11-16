#include "Scenarios/DivineBeastsDemoWorldState.h"
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"
#include "Definitions/GamePlatformWeatherPresetDefinition.h"
#include "Engine/GameInstance.h"
namespace DivineBeasts::Demo {
static FGamePlatformWeatherState ManualWeather(int32 Type,float Value){FGamePlatformWeatherState S;S.Type=static_cast<EGamePlatformWeatherType>(Type);switch(S.Type){case EGamePlatformWeatherType::LightRain:case EGamePlatformWeatherType::HeavyRain:case EGamePlatformWeatherType::Thunderstorm:S.RainIntensity=Value;S.Wetness=Value;break;case EGamePlatformWeatherType::LightSnow:case EGamePlatformWeatherType::HeavySnow:case EGamePlatformWeatherType::Blizzard:S.SnowIntensity=Value;S.SnowAmount=Value;S.TemperatureCelsius=-5;break;case EGamePlatformWeatherType::Fog:S.FogIntensity=Value;break;case EGamePlatformWeatherType::Wind:S.WindIntensity=Value;break;default:break;}return S;}
void FWorldManualState::WeatherOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 auto* S=World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();if(!S){D(Context.Token,WorldError(TEXT("Demo.Weather.Missing"),TEXT("真实WeatherRuntime服务不可用。")));return;}
 if(O==TEXT("Reset")){
  if(AtmosphereScope.IsValid()){auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();if(!C||!C->EndDevelopmentAtmosphere(AtmosphereScope)){D(Context.Token,WorldError(TEXT("Demo.Weather.AtmosphereCleanup"),TEXT("光雾作用域未确认恢复。")));return;}AtmosphereScope.Invalidate();}
  auto R=WeatherScope.IsValid()?WorldBool(S->EndDevelopmentWeather(WeatherScope),TEXT("EndDevelopmentWeather")):FGamePlatformResult::Success();if(R.IsSuccess()){WeatherScope.Invalidate();if(ProjectionScope.IsValid()){auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();R=WorldBool(C&&C->EndDevelopmentProjection(ProjectionScope),TEXT("EndDevelopmentProjection"));if(R.IsSuccess())ProjectionScope.Invalidate();}if(R.IsSuccess())R=ReleaseAsset();}D(Context.Token,R);return;
 }
 if(!ProjectionScope.IsValid()){auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();if(!C||!C->BeginDevelopmentProjection(Context.Token.RunId,ProjectionScope)){D(Context.Token,WorldError(TEXT("Demo.Weather.ProjectionBaselineRequired"),TEXT("需要默认天气表面/零风且无原生风源的独立开发World；无法完整恢复的既有投影不接管。")));return;}}
 if(!WeatherScope.IsValid()&&!S->BeginDevelopmentWeather(Context.Token.RunId,WeatherScope)){D(Context.Token,WorldError(TEXT("Demo.Weather.OwnershipRequired"),TEXT("只接管从未激活且无计划的Standalone开发天气；当前外部状态不能完整恢复，拒绝覆盖。")));return;}
 const auto E=Epoch;
 if(O==TEXT("ApplyPreset")){
  Load(A.Asset(TEXT("Preset")),[Weak=TWeakPtr<FWorldManualState>(AsShared())](UObject* V){auto P=Weak.Pin();auto* Preset=Cast<UGamePlatformWeatherPresetDefinition>(V);if(!P||!P->Alive()||!Preset)return WorldError(TEXT("Demo.Weather.PresetClass"),TEXT("资源不是天气预设。"));auto R=Preset->ValidateDefinition();if(!R.IsSuccess())return R;auto* W=P->World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();return WorldBool(W&&W->DevelopmentWeather(P->WeatherScope,TEXT("ApplyPreset"),Preset->Weather.State,{},0,Preset->Weather.TransitionSeconds),TEXT("ApplyPreset"));},MoveTemp(D));return;
 }
 FGamePlatformResult R=FGamePlatformResult::Success();
 if(O==TEXT("ConfigureAtmosphere")){
  auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();if(!C||(!AtmosphereScope.IsValid()&&!C->BeginDevelopmentAtmosphere(Context.Token.RunId,AtmosphereScope)))R=WorldError(TEXT("Demo.Weather.AtmosphereOwned"),TEXT("光雾已有外部启用配置或客户端服务不可用，拒绝接管。"));
  else{FGamePlatformWeatherAtmosphereProfile P;P.bEnabled=A.Bool(TEXT("Enabled"));P.AdditionalFogDensity=.025*A.Number(TEXT("FogScale"));P.OvercastSunMultiplier=A.Number(TEXT("LightScale"));R=WorldBool(C->ConfigureDevelopmentAtmosphere(AtmosphereScope,P),TEXT("ConfigureAtmosphere"));if(Current(E)&&R.IsSuccess())R=WorldBool(S->DevelopmentWeather(WeatherScope,TEXT("Activate"),ManualWeather(1,.4),{},0,0),TEXT("ActivateAtmosphereWeather"));}
 }else if(O==TEXT("RefreshPresentation")){
  auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();if(!C||S->GetCurrentSnapshot().Revision<=0)R=WorldError(TEXT("Demo.Weather.NoSnapshot"),TEXT("先在本Scope激活真实天气；没有快照不伪报刷新。"));else{C->RefreshPresentationAfterContentActivation(true);if(Current(E))C->RefreshPresentationAfterContentActivation(false);}
 }else{
  auto V=ManualWeather(static_cast<int32>(A.Int(TEXT("WeatherType"))),A.Number(TEXT("Intensity")));TArray<FGamePlatformWeatherScheduleEntry> Entries;
  if(O==TEXT("ConfigureSchedule")){FGamePlatformWeatherScheduleEntry Rain;Rain.State=ManualWeather(2,.4f);Rain.MinHoldSeconds=Rain.MaxHoldSeconds=A.Number(TEXT("HoldSeconds"));Rain.TransitionSeconds=A.Number(TEXT("Transition"));auto Clear=Rain;Clear.State=ManualWeather(0,0);Entries={Rain,Clear};}
  R=WorldBool(S->DevelopmentWeather(WeatherScope,O,V,Entries,static_cast<int32>(A.Int(TEXT("Seed"))),A.Number(TEXT("Transition"))),*O.ToString());
 }
 if(Current(E))Snapshot(O);D(Context.Token,Current(E)?R:FGamePlatformResult::Cancelled(TEXT("天气同步通知期间已停止。")));
}
void FWorldManualState::TelemetryOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 auto* S=Instance()?Instance()->GetSubsystem<UGamePlatformTelemetrySubsystem>():nullptr;
 if(!S||!TelemetryScope.IsValid()){D(Context.Token,WorldError(TEXT("Demo.Telemetry.Scope"),TEXT("开发遥测完整作用域不可用。")));return;}
 if(O==TEXT("RejectSecondScope")){FGuid Candidate;const bool Accepted=S->BeginDevelopmentCapture(*World,FGuid::NewGuid(),Candidate);if(Accepted)S->EndDevelopmentCapture(Candidate);D(Context.Token,WorldBool(!Accepted,TEXT("RejectSecondScope")));return;}
 EGamePlatformTelemetryRecordResult Recorded;FName Id=A.Id(TEXT("Result"));if(Id.IsNone())Id=A.Id(TEXT("SessionId"));if(O==TEXT("PublishWorld"))Id=A.Id(TEXT("WorldId"));if(O==TEXT("PublishCorrelation"))Id=A.Id(TEXT("CorrelationId"));if(Id.IsNone())Id=TEXT("synthetic");
 const FName ServiceOperation=O==TEXT("RecordSynthetic")?FName(TEXT("Counter")):O;
 const bool Executed=S->ExecuteDevelopmentCapture(TelemetryScope,ServiceOperation,A.Number(TEXT("Value")),Id.ToString(),Recorded);const auto V=S->GetDiagnostics();
 Observe(FString::Printf(TEXT("真实Telemetry：Operation=%s Executed=%d RecordResult=%d BufferDepth=%d；本开发sink只计合成批次，不读事件内容，不转发远端。Flush不等于网络送达。"),*O.ToString(),Executed,int32(Recorded),V.BufferDepth));
 if(O==TEXT("RejectSchema"))D(Context.Token,WorldBool(Executed&&Recorded==EGamePlatformTelemetryRecordResult::InvalidEventName,TEXT("RejectSchema")));
 else if(O==TEXT("RecordSynthetic")||O==TEXT("RecordEvent")||O==TEXT("Counter")||O==TEXT("Gauge")||O==TEXT("Histogram")||O==TEXT("Duration"))D(Context.Token,WorldBool(Executed&&Recorded==EGamePlatformTelemetryRecordResult::Recorded,*O.ToString()));
 else D(Context.Token,WorldBool(Executed,*O.ToString()));
}
}
