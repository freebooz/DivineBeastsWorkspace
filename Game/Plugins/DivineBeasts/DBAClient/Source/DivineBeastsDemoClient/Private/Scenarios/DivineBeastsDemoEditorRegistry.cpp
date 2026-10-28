// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#include "Scenarios/DivineBeastsDemoEditorScenarios.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
namespace DivineBeasts::Demo {
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterEditorManualScenarios(UDivineBeastsDemoCatalogSubsystem& C){
 TArray<FDivineBeastsDemoAdapterRegistrationHandle> Owned;for(FName F:EditorManualFeatures()){
  const auto H=C.RegisterScenarioAdapter(FName(*(TEXT("EditorManual.")+F.ToString())),[F](UWorld& W){return CreateEditorManualScenario(W,F);});
  if(!H.IsValid()){for(const auto& Prior:Owned)C.UnregisterScenarioAdapter(Prior);return {};}Owned.Add(H);
 }return Owned;
}
}
